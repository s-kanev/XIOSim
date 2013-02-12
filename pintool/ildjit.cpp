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
#include "BufferManager.h"

extern tick_t sim_cycle;
extern map<THREADID, Buffer> lookahead_buffer;
extern BufferManager handshake_buffer;

// True if ILDJIT has finished compilation and is executing user code
BOOL ILDJIT_executionStarted = false;

// True if a non-compiler thread is being created
BOOL ILDJIT_executorCreation = false;

XIOSIM_LOCK ildjit_lock;

const UINT8 ld_template[] = {0xa1, 0xad, 0xfb, 0xca, 0xde};
const UINT8 st_template[] = {0xc7, 0x05, 0xad, 0xfb, 0xca, 0xde, 0x00, 0x00, 0x00, 0x00 };

static map<string, UINT32> invocation_counts;

KNOB<BOOL> KnobDisableWaitSignal(KNOB_MODE_WRITEONCE,     "pintool",
        "disable_wait_signal", "false", "Don't insert any waits or signals into the pipeline");

static iteration_state_t phase_parent_id;
static iteration_state_t phase_start_id;
static iteration_state_t phase_end_id;

static string parent_loop = "";
static UINT32 parent_loop_invocation = -1;
static UINT32 parent_loop_iteration = -1;

static string start_loop = "";
static UINT32 start_loop_invocation = -1;
static UINT32 start_loop_iteration = -1;

static string end_loop = "";
static UINT32 end_loop_invocation = -1;
static UINT32 end_loop_iteration = -1;

VOID printMemoryUsage(THREADID tid);
VOID printElapsedTime();
BOOL signalCallback(THREADID tid, INT32 sig, CONTEXT *ctxt, BOOL hasHandler, const EXCEPTION_INFO *pExceptInfo, VOID *v);
void signalCallback2(int signum);
void initializePerThreadLoopState(THREADID tid);

bool loopMatches(string loop, UINT32 invocationNum, UINT32 iterationNum);
void readLoop(ifstream& fin, iteration_state_t* iteration_state);
void printLoop(iteration_state_t* iteration_state);

extern VOID doLateILDJITInstrumentation();
time_t last_time;

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

    readLoop(start_loop_file, &phase_start_id);
    readLoop(end_loop_file, &phase_end_id);
    
    phase_parent_id = phase_start_id;

    // hack for now
    parent_loop = phase_parent_id.name;
    parent_loop_invocation = phase_parent_id.invocationNumber;
    parent_loop_iteration = phase_parent_id.iterationNumber;
    
    start_loop = phase_start_id.name;
    start_loop_invocation = phase_start_id.invocationNumber;
    start_loop_iteration = phase_start_id.iterationNumber;
    
    end_loop = phase_end_id.name;
    end_loop_invocation = phase_end_id.invocationNumber;
    end_loop_iteration = phase_end_id.iterationNumber;
    
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

    printLoop(&phase_parent_id);    
    printLoop(&phase_start_id);
    printLoop(&phase_end_id);
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
  if (end_loop.size() != 0)
    return;

    // If we reach this, we're done with all parallel loops, just exit
//#ifdef ZESTO_PIN_DBG
    cerr << "Stopping simulation, TID: " << tid << endl;
//#endif

    if (KnobFluffy.Value().empty())
        StopSimulation(tid);
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

static UINT32 getSignalAddress(ADDRINT ssID);

static BOOL first_invocation = true;
static THREADID thread_started_invocation = -1;

static BOOL reached_parent_invocation = false;
static BOOL reached_start_invocation = false;
static BOOL reached_end_invocation = false;

static BOOL reached_start_iteration = false;
static BOOL reached_end_iteration = false;
static BOOL simulating_parallel_loop = false;

/* =========================================================================== */

VOID ILDJIT_startLoop(THREADID tid, ADDRINT ip, ADDRINT loop)
{
  // This is for when there are serial loops within an executing parallel loop.
  if (simulating_parallel_loop && reached_start_iteration) {
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

  // If at starting loop invocation and iteration...
  if((!reached_parent_invocation) && (parent_loop == loop_string) && (invocation_counts[loop_string] == parent_loop_invocation)) {
    assert(invocation_counts[loop_string] == parent_loop_invocation);
    cerr << "Called startLoop() for the parent invocation!:" << loop_string << endl;
    reached_parent_invocation = true;  
  }

  if((!reached_start_invocation) && reached_parent_invocation && (start_loop == loop_string) && (invocation_counts[loop_string] == start_loop_invocation)) {
    assert(invocation_counts[loop_string] == start_loop_invocation);
    cerr << "Called startLoop() for the start invocation!:" << loop_string << endl;
    reached_start_invocation = true;  
    if(start_loop_iteration == (UINT32)-1) {
      cerr << "Detected that we need to start the next parallel loop!:" << loop_string << endl;
      reached_start_iteration = true;
    }
  }

  if((!reached_end_invocation) && reached_parent_invocation && (end_loop == loop_string) && (invocation_counts[loop_string] == end_loop_invocation)) {
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
  if (simulating_parallel_loop && reached_start_iteration) {
    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = false;
    lk_unlock(&tstate->lock);    
  }
}

/* ========================================================================== */
VOID ILDJIT_startParallelLoop(THREADID tid, ADDRINT ip, ADDRINT loop, ADDRINT rc)
{
  disable_consumers();

  if(reached_start_invocation) {
    loop_states.push(loop_state_t());    
    loop_state = &(loop_states.top());
    loop_state->simmed_iteration_count = 0;
    loop_state->current_loop = loop;
    loop_state->invocationCount = invocation_counts[(string)(char*)loop];
    loop_state->iterationCount = -1;    
  }

  // If we didn't get to the start of the phase, return
  if(!reached_start_iteration) {
    return;
  }

  loop_state->use_ring_cache = (rc > 0);
  
  if(disable_wait_signal) {
    loop_state->use_ring_cache = false;
  }
  
  //#ifdef ZESTO_PIN_DBG
  CHAR* loop_name = (CHAR*) loop;
  cerr << "Starting loop: " << loop_name << "[" << invocation_counts[(string)(char*)loop] << "]" << endl;
  //#endif
  
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
    ResumeSimulation(tid);
  }
}
/* ========================================================================== */

// Assumes that start iteration calls _always_ happen in sequential order, 
// in a thread safe manner!  Have to take Simon's word on this one for now...
// Must be called from within the body of MOLECOOL_beforeWait!
VOID ILDJIT_startIteration(THREADID tid)
{ 
  if(!reached_parent_invocation) {
    return;
  }

  if((!reached_start_invocation) && (!reached_end_invocation)) {
    return;
  }

  loop_state->iterationCount++;
  
  //  cerr << (CHAR*)loop_state->current_loop << ":" << loop_state->invocationCount << ":" << loop_state->iterationCount << endl;

  // Check if this is the first iteration
  if((!reached_start_iteration) && loopMatches(start_loop, start_loop_invocation, start_loop_iteration)) {
    cerr << "SETTING REACHED START ITERATION" << endl;
    cerr << (CHAR*)loop_state->current_loop << endl;
    reached_start_iteration = true;
    loop_state->simmed_iteration_count = 0;
    thread_started_invocation = tid;

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
    cerr << "SETTING REACHED END ITERATION" << endl;

    assert(reached_parent_invocation && reached_start_invocation && reached_end_invocation && reached_start_iteration);

    PauseSimulation(tid);
    int iterCount = loop_state->simmed_iteration_count - 1;
    cerr << "Ending loop: anonymous" << " NumIterations:" << iterCount << endl;
    
    cerr << "Simulation runtime:";
    printElapsedTime();
    cerr << "LStopping simulation, TID: " << tid << endl;
    StopSimulation(tid);
    cerr << "[KEVIN] Stopped simulation! " << tid << endl;
  }   

  if(reached_start_iteration) {
    assert(reached_parent_invocation);
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

        vector<THREADID>::iterator it;
        for (it = thread_list.begin(); it != thread_list.end(); it++) {
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

    // Assumes all startIteration calls are sequential and within MOLECOOL_beforeWait()
    // don't need lock
    if(!reached_start_iteration) {
      return;
    }

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

    /* Not simulating -- just ignore. */
    if (tstate->ignore_all) {
        lk_unlock(&tstate->lock);
        goto cleanup;
    }

    lk_unlock(&tstate->lock);

    if (ExecMode != EXECUTION_MODE_SIMULATE)
        goto cleanup;

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

    /* Insert wait instruction in pipeline */
    handshake = lookahead_buffer[tid].get_buffer();

    handshake->flags.isFirstInsn = false;
    handshake->handshake.sleep_thread = false;
    handshake->handshake.resume_thread = false;
    handshake->handshake.real = false;
    handshake->handshake.coreID = tstate->coreID;
    handshake->handshake.in_critical_section = (num_threads > 1);
    handshake->flags.valid = true;

    handshake->handshake.pc = pc;
    handshake->handshake.npc = pc + sizeof(ld_template);
    handshake->handshake.tpc = pc + sizeof(ld_template);
    handshake->handshake.brtaken = false;
    memcpy(handshake->handshake.ins, ld_template, sizeof(ld_template));
    // Address comes right after opcode byte
//    ASSERTX(tstate->lastSignalAddr != 0xdecafbad);
    // set magic 17 bit in address - makes the pipeline see them as seperate addresses
    mask = 1 << 16;
    *(INT32*)(&handshake->handshake.ins[1]) = getSignalAddress(ssID) | mask; 
//    cerr << tid << ": Vodoo load instruction " << hex << pc <<  " ID: " << tstate->lastSignalAddr << dec << endl;
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

    // Assumes all startIteration calls are sequential and within MOLECOOL_beforeWait()
    // don't need lock
    if(!reached_start_iteration) {
      return;
    }

    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = false;

    /* Not simulating -- just ignore. */
    if (tstate->ignore_all) {
        lk_unlock(&tstate->lock);
        goto cleanup;
    }

    lk_unlock(&tstate->lock);

    #ifdef PRINT_WAITS
    lk_lock(&printing_lock, tid+1);
    if (ExecMode == EXECUTION_MODE_SIMULATE)
        cerr << tid <<": After Signal " << hex << pc << dec << endl;
    lk_unlock(&printing_lock);
    #endif

    /* Not simulating -- just ignore. */
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        goto cleanup;

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
    handshake->handshake.coreID = tstate->coreID;
    handshake->handshake.in_critical_section = (num_threads > 1) && (tstate->loop_state->unmatchedWaits > 0);
    handshake->flags.valid = true;

    handshake->handshake.pc = pc;
    handshake->handshake.npc = pc + sizeof(st_template);
    handshake->handshake.tpc = pc + sizeof(st_template);
    handshake->handshake.brtaken = false;
    memcpy(handshake->handshake.ins, st_template, sizeof(st_template));
    // Address comes right after opcode and MoodRM bytes
//    ASSERTX(tstate->lastSignalAddr != 0xdecafbad);
    *(INT32*)(&handshake->handshake.ins[2]) = getSignalAddress(ssID);

//    cerr << tid << ": Vodoo store instruction " << hex << pc << " ID: " << tstate->lastSignalAddr << dec << endl;
    lookahead_buffer[tid].push_done();

    flushLookahead(tid, 0);

cleanup:
    tstate->lastSignalAddr = 0xdecafbad;
}

/* ========================================================================== */
VOID ILDJIT_setAffinity(THREADID tid, INT32 coreID)
{
    ASSERTX(coreID >= 0 && coreID < num_threads);
    thread_state_t* tstate = get_tls(tid);
    tstate->coreID = coreID;
    core_threads[coreID] = tid;
    thread_cores[tid] = coreID;
    
    lookahead_buffer[tid] = Buffer(10);
    lookahead_buffer[tid].get_buffer()->flags.isFirstInsn = true;
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

     /**/






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
      fprintf(stderr, "MOLECOOL_startParallelLoop(): %p\n", RTN_Funptr(rtn));
	    
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
      fprintf(stderr, "MOLECOOL_startLoop(): %p\n", RTN_Funptr(rtn));
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

VOID printElapsedTime()
{
    time_t elapsed_time = time(NULL) - last_time;
    time_t hours = elapsed_time / 3600;
    time_t minutes = (elapsed_time % 3600) / 60;
    time_t seconds = ((elapsed_time % 3600) % 60);
    cerr << hours << "h" << minutes << "m" << seconds << "s" << endl;
    last_time = time(NULL);
}

VOID printMemoryUsage(THREADID tid)
{
    lk_lock(&printing_lock, tid+1);
    int myPid = getpid();
    char str[50];
    sprintf(str, "%d", myPid);

    ifstream fin;
    fin.open(("/proc/" + string(str) + "/status").c_str());
    string line;
    while(getline(fin, line)) {
        if(line.find("VmSize") != string::npos) {
            cerr << tid << ":" << line << endl;
            break;
        }
    }
    fin.close();
    lk_unlock(&printing_lock);
}

UINT32 getSignalAddress(ADDRINT ssID)
{
  UINT32 firstCore = 0;
  if(first_invocation) {
    firstCore = parent_loop_iteration % thread_cores.size();
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
  vector<THREADID>::iterator it;
  for (it = thread_list.begin(); it != thread_list.end(); it++) {
    thread_state_t* curr_tstate = get_tls(*it);
    lk_lock(&curr_tstate->lock, tid+1);
    curr_tstate->push_loop_state();
    lk_unlock(&curr_tstate->lock);
  }
}

void readLoop(ifstream& fin, iteration_state_t* iteration_state)
{
  string line;

  getline(fin, iteration_state->name);
  
  getline(fin, line);	
  iteration_state->invocationNumber = Uint32FromString(line);
  assert((UINT64)iteration_state->invocationNumber == Uint64FromString(line));

  getline(fin, line);	
  if(line == "-1") {
    iteration_state->iterationNumber = -1;
  }
  else {
    iteration_state->iterationNumber = Uint32FromString(line);
    assert((UINT64)iteration_state->iterationNumber == Uint64FromString(line));
  }
}

void printLoop(iteration_state_t* iteration_state)
{
  cerr << iteration_state->name << " ";
  cerr << iteration_state->invocationNumber << " ";
  cerr << iteration_state->iterationNumber << endl;
}
