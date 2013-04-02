/*
 * ILDJIT-specific functions for zesto feeder
 * Copyright, Svilen Kanev, 2012
 */

#include <stdlib.h>
#include <unistd.h>
#include <map>
#include <signal.h>
#include <queue>
#include <stack>
#include <signal.h>
#include "feeder.h"
#include "ildjit.h"
#include "fluffy.h"
#include "utils.h"
#include "scheduler.h"

#include "Buffer.h"
#include "BufferManager.h"

extern tick_t sim_cycle;
extern map<THREADID, Buffer> lookahead_buffer;

// True if ILDJIT has finished compilation and is executing user code
BOOL ILDJIT_executionStarted = false;

// True if a non-compiler thread is being created
BOOL ILDJIT_executorCreation = false;

XIOSIM_LOCK ildjit_lock;

const UINT8 ld_template[] = {0xa1, 0xad, 0xfb, 0xca, 0xde};
const UINT8 st_template[] = {0xc7, 0x05, 0xad, 0xfb, 0xca, 0xde, 0x00, 0x00, 0x00, 0x00 };
const UINT8 syscall_template[] = {0xcd, 0x80};


static map<string, UINT32> invocation_counts;

KNOB<BOOL> KnobDisableWaitSignal(KNOB_MODE_WRITEONCE,     "pintool",
        "disable_wait_signal", "false", "Don't insert any waits or signals into the pipeline");

static string start_loop = "";
static UINT32 start_loop_invocation = -1;
static UINT32 start_loop_iteration = -1;

static string end_loop = "";
static UINT32 end_loop_invocation = -1;
static UINT32 end_loop_iteration = -1;

static BOOL first_invocation = true;

static BOOL reached_start_invocation = false;
static BOOL reached_end_invocation = false;

static BOOL reached_start_iteration = false;
static BOOL reached_end_iteration = false;
static BOOL simulating_parallel_loop = false;

UINT32 getSignalAddress(ADDRINT ssID);
BOOL signalCallback(THREADID tid, INT32 sig, CONTEXT *ctxt, BOOL hasHandler, const EXCEPTION_INFO *pExceptInfo, VOID *v);
void signalCallback2(int signum);
void initializePerThreadLoopState(THREADID tid);

bool loopMatches(string loop, UINT32 invocationNum, UINT32 iterationNum);
void readLoop(ifstream& fin, string* name, UINT32* invocation, UINT32* iteration);

extern VOID doLateILDJITInstrumentation();

stack<loop_state_t> loop_states;
loop_state_t* loop_state;

bool disable_wait_signal;
UINT32* ildjit_ws_id;
UINT32* ildjit_disable_ws;

/* ========================================================================== */
VOID MOLECOOL_Init()
{
    lk_init(&ildjit_lock);

    FLUFFY_Init();

    ifstream start_loop_file, end_loop_file;
    start_loop_file.open("phase_start_loop", ifstream::in);
    end_loop_file.open("phase_end_loop", ifstream::in);

    if (start_loop_file.fail()) {
      cerr << "Couldn't open loop id files: phase_start_loop" << endl;
      PIN_ExitProcess(1);
    }
    if (end_loop_file.fail()) {
      cerr << "Couldn't open loop id files: phase_end_loop" << endl;
      PIN_ExitProcess(1);
    }

    readLoop(start_loop_file, &start_loop, &start_loop_invocation, &start_loop_iteration);
    readLoop(end_loop_file, &end_loop, &end_loop_invocation, &end_loop_iteration);

    disable_wait_signal = KnobDisableWaitSignal.Value();

    // callbacks so we can delete temporary files in /dev/shm
    PIN_InterceptSignal(SIGINT, signalCallback, NULL);
    PIN_InterceptSignal(SIGABRT, signalCallback, NULL);
    PIN_InterceptSignal(SIGFPE, signalCallback, NULL);
    PIN_InterceptSignal(SIGILL, signalCallback, NULL);
    PIN_InterceptSignal(SIGSEGV, signalCallback, NULL);
    PIN_InterceptSignal(SIGTERM, signalCallback, NULL);
    PIN_InterceptSignal(SIGKILL, signalCallback, NULL);

    signal(SIGINT, signalCallback2);
    signal(SIGABRT, signalCallback2);
    signal(SIGFPE, signalCallback2);
    signal(SIGILL, signalCallback2);
    signal(SIGSEGV, signalCallback2);
    signal(SIGTERM, signalCallback2);
    signal(SIGKILL, signalCallback2);

    last_time = time(NULL);

    cerr << start_loop << endl;
    cerr << start_loop_invocation << endl;
    cerr << start_loop_iteration << endl << endl;
    cerr << end_loop << endl;
    cerr << end_loop_invocation << endl;
    cerr << end_loop_iteration << endl << endl;
}

/* ========================================================================== */
BOOL ILDJIT_IsExecuting()
{
    bool res;
    lk_lock(&ildjit_lock, 1);
    res = ILDJIT_executionStarted;
    lk_unlock(&ildjit_lock);
    return res;
}

/* ========================================================================== */
BOOL ILDJIT_IsCreatingExecutor()
{
    bool res;
    lk_lock(&ildjit_lock, 1);
    res = ILDJIT_executorCreation;
    lk_unlock(&ildjit_lock);
    return res;
}

/* ========================================================================== */
VOID ILDJIT_startSimulation(THREADID tid, ADDRINT ip)
{
    CODECACHE_FlushCache();

    lk_lock(&ildjit_lock, 1);

    /* We are stopping thread creation here, beacuse we can capture the real
     * thread creation in Pin only on starting the thread (first insn), which
     * happens after the actual syscalls.
     * XXX: This way we can capture the creation of some compiler threads,
     * but this is generally fine, since they won't get executed */
    ILDJIT_executorCreation = false;

    ILDJIT_executionStarted = true;

//#ifdef ZESTO_PIN_DBG
    cerr << "Starting execution, TID: " << tid << endl;
//#endif

    lk_unlock(&ildjit_lock);
}

/* ========================================================================== */
VOID ILDJIT_setupInterface(ADDRINT disable_ws, ADDRINT ws_id)
{
  ildjit_ws_id = (UINT32*)ws_id;
  ildjit_disable_ws = (UINT32*)disable_ws;
}

/* ========================================================================== */

VOID ILDJIT_endSimulation(THREADID tid, ADDRINT ip)
{
  // This should cover the helix case
  if (end_loop.size() != 0)
    return;

    // If we reach this, we're done with all parallel loops, just exit
//#ifdef ZESTO_PIN_DBG
    cerr << "Stopping simulation, TID: " << tid << endl;
//#endif

    if (KnobFluffy.Value().empty())
        PauseSimulation(tid);
}

/* ========================================================================== */
VOID ILDJIT_ExecutorCreate(THREADID tid)
{
    lk_lock(&ildjit_lock, 1);

    ILDJIT_executorCreation = true;

//#ifdef ZESTO_PIN_DBG
    cerr << "Starting creation, TID: " << tid << endl;
//#endif

    lk_unlock(&ildjit_lock);
}

/* ========================================================================== */
VOID ILDJIT_ExecutorCreateEnd(THREADID tid)
{
    //Dummy, actual work now done in ILDJIT_ThreadStarting
}

/* =========================================================================== */

VOID ILDJIT_startLoop(THREADID tid, ADDRINT ip, ADDRINT loop)
{
  // This is for when there are serial loops within an executing parallel loop.
  if (simulating_parallel_loop) {
    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = true;
    lk_unlock(&tstate->lock);

    int numInstsToIgnore = 2; // call and param
    flushLookahead(tid, numInstsToIgnore);
  }

  string loop_string = (string)(char*)loop;

  // Increment invocation counter for this loop
  if(invocation_counts.count(loop_string) == 0) {
    invocation_counts[loop_string] = 0;
  }
  else {
    invocation_counts[loop_string]++;
  }

  if((!reached_start_invocation) && (start_loop == loop_string) && (invocation_counts[loop_string] == start_loop_invocation)) {
    assert(invocation_counts[loop_string] == start_loop_invocation);
    cerr << "Called startLoop() for the start invocation!:" << loop_string << endl;
    reached_start_invocation = true;
    if(start_loop_iteration == (UINT32)-1) {
      cerr << "Detected that we need to start the next parallel loop!:" << loop_string << endl;
      reached_start_iteration = true;
    }
  }

  if((!reached_end_invocation) && (end_loop == loop_string) && (invocation_counts[loop_string] == end_loop_invocation)) {
    assert(invocation_counts[loop_string] == end_loop_invocation);
    cerr << "Called startLoop() for the end invocation!:" << (CHAR*)loop << endl;
    reached_end_invocation = true;
    if(end_loop_iteration == (UINT32)-1) {
      cerr << "Detected that we need to end the next parallel loop!:" << loop_string << endl;
      reached_end_iteration = true;
    }
  }
}

/* ========================================================================== */
VOID ILDJIT_startLoop_after(THREADID tid, ADDRINT ip)
{
  // This is for when there are serial loops within an executing parallel loop.
  if (simulating_parallel_loop) {
    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = false;
    lk_unlock(&tstate->lock);
  }
}

/* ========================================================================== */
VOID ILDJIT_startParallelLoop(THREADID tid, ADDRINT ip, ADDRINT loop, ADDRINT rc)
{
  if(reached_start_invocation) {
    loop_states.push(loop_state_t());
    loop_state = &(loop_states.top());
    loop_state->simmed_iteration_count = 0;
    loop_state->current_loop = loop;
    loop_state->invocationCount = invocation_counts[(string)(char*)loop];
    loop_state->iterationCount = -1;

    CHAR* loop_name = (CHAR*) loop;
    cerr << "Starting loop: " << loop_name << "[" << invocation_counts[(string)(char*)loop] << "]" << endl;
  }

  // If we didn't get to the start of the phase, return
  if(!reached_start_iteration) {
    return;
  }

  loop_state->use_ring_cache = (rc > 0);

  if(disable_wait_signal) {
    loop_state->use_ring_cache = false;
  }

  assert(reached_start_iteration);

  initializePerThreadLoopState(tid);
  simulating_parallel_loop = true;

  if (ExecMode != EXECUTION_MODE_SIMULATE) {
    cerr << "Do late!" << endl;
    doLateILDJITInstrumentation();
    cerr << "Done late!" << endl;

    cerr << "FastForward runtime:";
    printElapsedTime();

    cerr << "Starting simulation, TID: " << tid << endl;
    PPointHandler(CONTROL_START, NULL, NULL, NULL, tid);
    first_invocation = false;
  }
  else {
    cerr << tid << ": resuming simulation" << endl;
    ILDJIT_ResumeSimulation(tid);
  }
}
/* ========================================================================== */

// Assumes that start iteration calls _always_ happen in sequential order,
// in a thread safe manner!  Have to take Simon's word on this one for now...
// Must be called from within the body of MOLECOOL_beforeWait!
VOID ILDJIT_startIteration(THREADID tid)
{
  if((!reached_start_invocation) && (!reached_end_invocation)) {
    return;
  }

  loop_state->iterationCount++;

  // Check if this is the first iteration
  if((!reached_start_iteration) && loopMatches(start_loop, start_loop_invocation, start_loop_iteration)) {
    reached_start_iteration = true;
    loop_state->simmed_iteration_count = 0;

    cerr << "Do late!" << endl;
    doLateILDJITInstrumentation();
    cerr << "Done late!" << endl;

    cerr << "FastForward runtime:";
    printElapsedTime();

    cerr << "Starting simulation, TID: " << tid << endl;
    initializePerThreadLoopState(tid);

    simulating_parallel_loop = true;
    PPointHandler(CONTROL_START, NULL, NULL, NULL, tid);
  }

    // Check if this is the last iteration
  if(reached_end_iteration || loopMatches(end_loop, end_loop_invocation, end_loop_iteration)) {
    assert(reached_start_invocation && reached_end_invocation && reached_start_iteration);

    ILDJIT_PauseSimulation(tid);
    int iterCount = loop_state->simmed_iteration_count - 1;
    cerr << "Ending loop: anonymous" << " NumIterations:" << iterCount << endl;

    cerr << "Simulation runtime:";
    printElapsedTime();
    cerr << "Stopping simulation, TID: " << tid << endl;
    Zesto_Slice_End(0, 1, 0, 100*1000);
    StopSimulation(true);
    cerr << "[KEVIN] Stopped simulation! " << tid << endl;
    PIN_ExitProcess(EXIT_SUCCESS);
  }

  if(reached_start_iteration) {
    assert(reached_start_invocation);
    loop_state->simmed_iteration_count++;
  }
}

/* ========================================================================== */
VOID ILDJIT_endParallelLoop(THREADID tid, ADDRINT loop, ADDRINT numIterations)
{
#ifdef ZESTO_PIN_DBG
    cerr << tid << ": Pausing simulation!" << endl;
#endif
    if (ExecMode == EXECUTION_MODE_SIMULATE) {
        // flush a call, two parameter stores
        int numInstsToIgnore = 3;
        flushLookahead(tid, numInstsToIgnore);

        PauseSimulation(tid);
        cerr << tid << ": Paused simulation!" << endl;

        first_invocation = false;

        list<THREADID>::iterator it;
        ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
            thread_state_t* tstate = get_tls(*it);
            lk_lock(&tstate->lock, tid+1);
            tstate->ignore = true;
            tstate->pop_loop_state();
            lk_unlock(&tstate->lock);
        }

        CHAR* loop_name = (CHAR*) loop;
        UINT32 iterCount = loop_state->simmed_iteration_count - 1;
        cerr << "Ending loop: " << loop_name << " NumIterations:" << iterCount << endl;
        simulating_parallel_loop = false;

        assert(loop_states.size() > 0);
        loop_states.pop();
        if(loop_states.size() > 0) {
          loop_state = &(loop_states.top());
        }
    }
}

/* ========================================================================== */
VOID ILDJIT_beforeWait(THREADID tid, ADDRINT ssID, ADDRINT pc)
{
#ifdef PRINT_WAITS
  lk_lock(&printing_lock, tid+1);
  if (ExecMode == EXECUTION_MODE_SIMULATE)
    cerr << tid <<" :Before Wait "<< hex << pc << dec  << " ID: " << dec << ssID << endl;
  lk_unlock(&printing_lock);
#endif

    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = true;
    lk_unlock(&tstate->lock);

    int numInstsToIgnore = 2; // flush a call, one parameter
    flushLookahead(tid, numInstsToIgnore);

    if(num_threads == 1) {
        return;
    }

    tstate->lastWaitID = ssID;
}

/* ========================================================================== */
VOID ILDJIT_afterWait(THREADID tid, ADDRINT ssID, ADDRINT is_light, ADDRINT pc)
{
    assert(ssID < 256);
    thread_state_t* tstate = get_tls(tid);
    handshake_container_t* handshake;
    int mask;
    bool first_insn;

    if (ExecMode != EXECUTION_MODE_SIMULATE)
      goto cleanup;

    lk_lock(&tstate->lock, tid+1);

    tstate->ignore = false;

#ifdef PRINT_WAITS
    lk_lock(&printing_lock, tid+1);
    if (ExecMode == EXECUTION_MODE_SIMULATE)
      cerr << tid <<": After Wait "<< hex << pc << dec  << " ID: " << tstate->lastWaitID << ":" << ssID << endl;
    lk_unlock(&printing_lock);
#endif

    // Indicates not in a wait any more
    tstate->lastWaitID = -1;

    first_insn = tstate->firstInstruction;

    /* Not simulating -- just ignore. */
    if (tstate->ignore_all) {
        lk_unlock(&tstate->lock);
        goto cleanup;
    }

    lk_unlock(&tstate->lock);

    /* Don't insert waits in single-core mode */
    if (num_threads == 1)
        goto cleanup;

    tstate->loop_state->unmatchedWaits++;

    /* Ignore injecting waits until the end of the first iteration,
     * so we can start simulating */

    assert(loop_state->simmed_iteration_count > 0);
    if (loop_state->simmed_iteration_count == 1) {
      goto cleanup;
    }

    if(!(loop_state->use_ring_cache)) {
      goto cleanup;
    }

    if(is_light) {
        goto cleanup;
    }

    /* We're not a first instruction any more */
    if (first_insn) {
        lk_lock(&tstate->lock, tid+1);
        tstate->firstInstruction = false;
        lk_unlock(&tstate->lock);
    }

    /* Insert wait instruction in pipeline */
    handshake = lookahead_buffer[tid].get_buffer();

    handshake->flags.isFirstInsn = first_insn;
    handshake->handshake.sleep_thread = false;
    handshake->handshake.resume_thread = false;
    handshake->handshake.real = false;
    handshake->handshake.in_critical_section = (num_threads > 1);
    handshake->flags.valid = true;

    handshake->handshake.pc = pc;
    handshake->handshake.npc = pc + sizeof(ld_template);
    handshake->handshake.tpc = pc + sizeof(ld_template);
    handshake->handshake.brtaken = false;
    memcpy(handshake->handshake.ins, ld_template, sizeof(ld_template));
    // Address comes right after opcode byte
    // set magic 17 bit in address - makes the pipeline see them as seperate addresses
    mask = 1 << 16;
    *(INT32*)(&handshake->handshake.ins[1]) = getSignalAddress(ssID) | mask;
    lookahead_buffer[tid].push_done();

    flushLookahead(tid, 0);

cleanup:
    tstate->lastSignalAddr = 0xdecafbad;
}

/* ========================================================================== */
VOID ILDJIT_beforeSignal(THREADID tid, ADDRINT ssID, ADDRINT pc)
{
    thread_state_t* tstate = get_tls(tid);

    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = true;
    lk_unlock(&tstate->lock);
#ifdef PRINT_WAITS
    lk_lock(&printing_lock, tid+1);
    if (ExecMode == EXECUTION_MODE_SIMULATE)
      cerr << tid <<": Before Signal " << hex << pc << " ID: " << ssID << dec << endl;
    lk_unlock(&printing_lock);
#endif

    int numInstsToIgnore = 2; // flush a call, one parameter
    flushLookahead(tid, numInstsToIgnore);
//    ASSERTX(tstate->lastSignalAddr == 0xdecafbad);
}

/* ========================================================================== */
VOID ILDJIT_afterSignal(THREADID tid, ADDRINT ssID, ADDRINT pc)
{
  assert(ssID < 256);
    thread_state_t* tstate = get_tls(tid);
    handshake_container_t* handshake;

    /* Not simulating -- just ignore. */
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        goto cleanup;


    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = false;

    /* Not simulating -- just ignore. */
    if (tstate->ignore_all) {
        lk_unlock(&tstate->lock);
        goto cleanup;
    }

    assert(!tstate->firstInstruction);
    lk_unlock(&tstate->lock);

    #ifdef PRINT_WAITS
    lk_lock(&printing_lock, tid+1);
    if (ExecMode == EXECUTION_MODE_SIMULATE)
        cerr << tid <<": After Signal " << hex << pc << dec << endl;
    lk_unlock(&printing_lock);
    #endif


    /* Don't insert signals in single-core mode */
    if (num_threads == 1)
        goto cleanup;

    tstate->loop_state->unmatchedWaits--;
    ASSERTX(tstate->loop_state->unmatchedWaits >= 0);

    if(!(loop_state->use_ring_cache)) {
      return;
    }

    /* Insert signal instruction in pipeline */
    handshake = lookahead_buffer[tid].get_buffer();

    handshake->flags.isFirstInsn = false;
    handshake->handshake.sleep_thread = false;
    handshake->handshake.resume_thread = false;
    handshake->handshake.real = false;
    handshake->handshake.in_critical_section = (num_threads > 1) && (tstate->loop_state->unmatchedWaits > 0);
    handshake->flags.valid = true;

    handshake->handshake.pc = pc;
    handshake->handshake.npc = pc + sizeof(st_template);
    handshake->handshake.tpc = pc + sizeof(st_template);
    handshake->handshake.brtaken = false;
    memcpy(handshake->handshake.ins, st_template, sizeof(st_template));
    // Address comes right after opcode and MoodRM bytes
    *(INT32*)(&handshake->handshake.ins[2]) = getSignalAddress(ssID);

    lookahead_buffer[tid].push_done();

    flushLookahead(tid, 0);

cleanup:
    tstate->lastSignalAddr = 0xdecafbad;
}

/* ========================================================================== */
VOID ILDJIT_setAffinity(THREADID tid, INT32 coreID)
{
    ASSERTX(coreID >= 0 && coreID < num_threads);
    HardcodeSchedule(tid, coreID);

    lookahead_buffer[tid] = Buffer(10);
}

/* ========================================================================== */
VOID AddILDJITCallbacks(IMG img)
{
#ifdef ZESTO_PIN_DBG
    cerr << "Adding ILDJIT callbacks: ";
#endif

    //Interface to ildjit
    RTN rtn;
    rtn = RTN_FindByName(img, "MOLECOOL_codeExecutorCreation");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_codeExecutorCreation ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_ExecutorCreate),
                       IARG_THREAD_ID,
                       IARG_END);
        RTN_Close(rtn);
    }

    /**/

    rtn = RTN_FindByName(img, "MOLECOOL_setupInterface");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_setupInterface ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_setupInterface),
                       IARG_THREAD_ID,
                       IARG_END);
        RTN_Close(rtn);
    }



    rtn = RTN_FindByName(img, "MOLECOOL_startIteration");
    if (RTN_Valid(rtn))
    {
      fprintf(stderr, "MOLECOOL_startIteration(): %p\n", RTN_Funptr(rtn));
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_startIteration ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_startIteration),
                       IARG_THREAD_ID,
                       IARG_END);
        RTN_Close(rtn);
    }


    rtn = RTN_FindByName(img, "MOLECOOL_beforeWait");
    if (RTN_Valid(rtn))
    {
      fprintf(stderr, "MOLECOOL_beforeWait(): %p\n", RTN_Funptr(rtn));
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_beforeWait),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_INST_PTR,
                       IARG_CALL_ORDER, CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_afterWait");
    if (RTN_Valid(rtn))
    {
      fprintf(stderr, "MOLECOOL_afterWait(): %p\n", RTN_Funptr(rtn));
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(ILDJIT_afterWait),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_INST_PTR,
                       IARG_CALL_ORDER, CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_beforeSignal");
    if (RTN_Valid(rtn))
    {
      fprintf(stderr, "MOLECOOL_beforeSignal(): %p\n", RTN_Funptr(rtn));
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_beforeSignal),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_INST_PTR,
                       IARG_CALL_ORDER, CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_afterSignal");
    if (RTN_Valid(rtn))
    {
      fprintf(stderr, "MOLECOOL_afterSignal(): %p\n", RTN_Funptr(rtn));
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(ILDJIT_afterSignal),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_INST_PTR,
                       IARG_CALL_ORDER, CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_endParallelLoop");
    if (RTN_Valid(rtn))
    {
      fprintf(stderr, "MOLECOOL_endParallelLoop(): %p\n", RTN_Funptr(rtn));
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_endParallelLoop),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_codeExecutorCreationEnd");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_codeExecutorCreationEnd ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_ExecutorCreateEnd),
                       IARG_THREAD_ID,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_startParallelLoop");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_startParallelLoop ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(ILDJIT_startParallelLoop),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_startLoop");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_startLoop ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_startLoop),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_CALL_ORDER, CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_startLoop");
   if (RTN_Valid(rtn))
      {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_startLoop ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(ILDJIT_startLoop_after),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_CALL_ORDER, CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }



    rtn = RTN_FindByName(img, "MOLECOOL_startSimulation");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_startSimulation ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_startSimulation),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_endSimulation");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_endSimulation ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_endSimulation),
                       IARG_THREAD_ID,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_setAffinity");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_setAffinity ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(ILDJIT_setAffinity),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_CALL_ORDER, CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
    }

//==========================================================
//FLUFFY-related
    if (!KnobFluffy.Value().empty())
    {

        rtn = RTN_FindByName(img, "step3_start_inst");
        if (RTN_Valid(rtn))
        {
#ifdef ZESTO_PIN_DBG
            cerr << "FLUFFY_step3_start_inst ";
#endif
            RTN_Open(rtn);

            RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(FLUFFY_StartInsn),
                           IARG_THREAD_ID,
                           IARG_INST_PTR,
                           IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                           IARG_END);

            RTN_Close(rtn);
        }

        rtn = RTN_FindByName(img, "step3_end_inst");
        if (RTN_Valid(rtn))
        {
#ifdef ZESTO_PIN_DBG
            cerr << "FLUFFY_step3_end_inst ";
#endif
            RTN_Open(rtn);

            RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(FLUFFY_StopInsn),
                           IARG_THREAD_ID,
                           IARG_INST_PTR,
                           IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                           IARG_END);

            RTN_Close(rtn);
        }
    }
#ifdef ZESTO_PIN_DBG
    cerr << endl;
#endif
}

BOOL signalCallback(THREADID tid, INT32 sig, CONTEXT *ctxt, BOOL hasHandler, const EXCEPTION_INFO *pExceptInfo, VOID *v)
{
    handshake_buffer.signalCallback(sig);
    PIN_ExitProcess(1);
    return false;
}

void signalCallback2(int signum)
{
    handshake_buffer.signalCallback(signum);
    PIN_ExitProcess(1);
}

UINT32 getSignalAddress(ADDRINT ssID)
{
  UINT32 firstCore = 0;
  if(first_invocation) {
    firstCore = start_loop_iteration % num_threads;
  }
  assert(firstCore < 256);
  assert(ssID < 256);

  return 0x7ffe0000 + (firstCore << 8) + ssID;
}

bool loopMatches(string loop, UINT32 invocationNum, UINT32 iterationNum)
{
  if(loop_state->invocationCount != invocationNum) {
    return false;
  }

  if(loop_state->iterationCount != iterationNum) {
    return false;
  }

  if(string((char*)loop_state->current_loop) != loop) {
    return false;
  }

  return true;
}

void initializePerThreadLoopState(THREADID tid)
{
  list<THREADID>::iterator it;
  ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
    thread_state_t* curr_tstate = get_tls(*it);
    lk_lock(&curr_tstate->lock, tid+1);
    curr_tstate->push_loop_state();
    lk_unlock(&curr_tstate->lock);
  }
}

void readLoop(ifstream& fin, string* name, UINT32* invocation, UINT32* iteration)
{
  string line;

  getline(fin, *name);

  getline(fin, line);
  *invocation = Uint32FromString(line);
  assert((UINT64)(*invocation) == Uint64FromString(line));

  getline(fin, line);
  if(line == "-1") {
    *iteration = -1;
  }
  else {
    *iteration = Uint32FromString(line);
    assert((UINT64)(*iteration) == Uint64FromString(line));
  }
}

/* ========================================================================== */
VOID ILDJIT_PauseSimulation(THREADID tid)
{
    /* The context is that all cores functionally have sent signal 0
     * and unblocked the last iteration. We need to (i) wait for them
     * to functionally reach wait 0, where they will wait until the end
     * of the loop; (ii) drain all pipelines once cores are waiting. */
    cerr << "pausing..." << endl;
    flushLookahead(tid, 0);

    volatile bool done_with_iteration = false;
    do {
        done_with_iteration = true;
        list<THREADID>::iterator it;
        ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
            if ((*it) != tid) {
                thread_state_t* tstate = get_tls(*it);
                lk_lock(&tstate->lock, tid + 1);
                bool curr_done = tstate->ignore && (tstate->lastWaitID == 0);
                if(curr_done) {
                    assert(lookahead_buffer[*it].size() == 0);
                    assert(lookahead_buffer[*it].empty());
                }
                done_with_iteration &= curr_done;
                /* Setting ignore_all here (while ignore is set) should be a race-free way
                 * of ignoring the serial portion outside the loop after the thread goes
                 * on an unsets ignore locally. */
                if (curr_done) {
                    tstate->ignore_all = true;
                }
                lk_unlock(&tstate->lock);
            }
        }
    } while (!done_with_iteration);

    /* Here we have produced everything for this loop! */
    disable_producers();
    enable_consumers();

    /* Drainning all pipelines and deactivating cores. */
    list<THREADID>::iterator it;
    ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
        /* Insert a trap. This will ensure that the pipe drains before
         * consuming the next instruction.*/
        handshake_container_t* handshake = handshake_buffer.get_buffer(*it);
        handshake->flags.isFirstInsn = false;
        handshake->handshake.sleep_thread = false;
        handshake->handshake.resume_thread = false;
        handshake->handshake.real = false;
        handshake->flags.valid = true;

        handshake->handshake.pc = (ADDRINT) syscall_template;
        handshake->handshake.npc = (ADDRINT) syscall_template + sizeof(syscall_template);
        handshake->handshake.tpc = (ADDRINT) syscall_template + sizeof(syscall_template);
        handshake->handshake.brtaken = false;
        memcpy(handshake->handshake.ins, syscall_template, sizeof(syscall_template));
        handshake_buffer.producer_done(*it, true);

        /* Deactivate this core, so we can advance the cycle conunter of
         * others without waiting on it */
        handshake_container_t* handshake_2 = handshake_buffer.get_buffer(*it);

        handshake_2->flags.isFirstInsn = false;
        handshake_2->handshake.sleep_thread = true;
        handshake_2->handshake.resume_thread = false;
        handshake_2->handshake.real = false;
        handshake_2->handshake.pc = 0;
        handshake_2->flags.valid = true;
        handshake_buffer.producer_done(*it, true);

        /* And finally, flush the core's pipelie to get rid of anything
         * left over (including the trap) and flush the ring cache */
        handshake_container_t* handshake_3 = handshake_buffer.get_buffer(*it);

        handshake_3->flags.isFirstInsn = false;
        handshake_3->handshake.sleep_thread = false;
        handshake_3->handshake.resume_thread = false;
        handshake_3->handshake.flush_pipe = true;
        handshake_3->handshake.real = false;
        handshake_3->handshake.pc = 0;
        handshake_3->flags.valid = true;
        handshake_buffer.producer_done(*it, true);

        handshake_buffer.flushBuffers(*it);
    }

    enable_consumers();

    /* Wait until all cores are done -- consumed their buffers. */
    volatile bool done = false;
    do {
        done = true;
        list<THREADID>::iterator it;
        ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
            done &= handshake_buffer.empty((*it));
        }
        if (!done && sleeping_enabled && (host_cpus <= num_threads))
            PIN_Sleep(10);
    } while (!done);

#ifdef ZESTO_PIN_DBG
    cerr << tid << " [" << sim_cycle << ":KEVIN]: All cores have empty buffers" << endl;
    cerr.flush();
#endif

    ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
      thread_state_t* tstate = get_tls(*it);
      cerr << tstate->coreID << ":OverlapCycles:" << sim_cycle - lastConsumerApply[*it] << endl;
    }

    disable_consumers();
    enable_producers();

    /* Have thread ignore serial section after */
    /* XXX: Do we need this? A few lines above we set ignore_all! */
    ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
        thread_state_t* tstate = get_tls(*it);
        lk_lock(&tstate->lock, *it+1);
        handshake_buffer.resetPool(*it);
        tstate->ignore = true;
        lk_unlock(&tstate->lock);
        assert(lookahead_buffer[*it].empty());
    }
}

/* ========================================================================== */
VOID ILDJIT_ResumeSimulation(THREADID tid)
{

    /* All cores were sleeping in between loops, wake them up now. */
    for (INT32 coreID = 0; coreID < num_threads; coreID++) {
        /* Wake up cores right away without going through the handshake
         * buffer (which should be empty anyway).
         * If we do go through it, there are no guarantees for when the
         * resume is consumed, which can lead to nasty races of who gets
         * to resume first. */
        THREADID curr_tid = GetCoreThread(coreID);
        if (curr_tid == INVALID_THREADID)
            continue;

        ASSERTX(handshake_buffer.empty(curr_tid));
        ASSERTX(lookahead_buffer[curr_tid].empty());
        activate_core(coreID);
        thread_state_t* tstate = get_tls(curr_tid);
        lk_lock(&tstate->lock, tid+1);
        tstate->ignore_all = false;
        lk_unlock(&tstate->lock);
    }
}
