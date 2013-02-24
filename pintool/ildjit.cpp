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

// True if ILDJIT has finished compilation and is executing user code
BOOL ILDJIT_executionStarted = false;

// True if a non-compiler thread is being created
BOOL ILDJIT_executorCreation = false;

XIOSIM_LOCK ildjit_lock;

const UINT8 ld_template[] = {0xa1, 0xad, 0xfb, 0xca, 0xde};
const UINT8 st_template[] = {0xc7, 0x05, 0xad, 0xfb, 0xca, 0xde, 0x00, 0x00, 0x00, 0x00 };
const UINT8 syscall_template[] = {0xcd, 0x80};


static map<ADDRINT, UINT32> invocation_counts;
static map<ADDRINT, UINT32> iteration_counts;

KNOB<string> KnobLoopIDFile(KNOB_MODE_WRITEONCE, "pintool",
    "loopID", "", "File to get start/end loop IDs and invocation caounts");

KNOB<BOOL> KnobDisableWaitSignal(KNOB_MODE_WRITEONCE,     "pintool",
        "disable_wait_signal", "false", "Don't insert any waits or signals into the pipeline");

static string start_loop = "";
static UINT32 start_loop_invocation = -1;
static UINT32 start_loop_iteration = -1;

static string end_loop = "";
static UINT32 end_loop_invocation = -1;
static UINT32 end_loop_iteration = -1;

BOOL signalCallback(THREADID tid, INT32 sig, CONTEXT *ctxt, BOOL hasHandler, const EXCEPTION_INFO *pExceptInfo, VOID *v);
void signalCallback2(int signum);

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

    if (!KnobLoopIDFile.Value().empty()) {
        ifstream loop_file;
        loop_file.open(KnobLoopIDFile.Value().c_str(), ifstream::in);
        if (loop_file.fail()) {
            cerr << "Couldn't open loop id file: " << KnobLoopIDFile.Value() << endl;
            PIN_ExitProcess(1);
        }

        string line;
        // start phase info
        getline(loop_file, start_loop);

        getline(loop_file, line);
        start_loop_invocation = Uint32FromString(line);
        assert((UINT64)start_loop_invocation == Uint64FromString(line));

        getline(loop_file, line);
        start_loop_iteration = Uint32FromString(line);
        assert((UINT64)start_loop_iteration == Uint64FromString(line));

        // end phase info
        getline(loop_file, end_loop);

        getline(loop_file, line);
        end_loop_invocation = Uint32FromString(line);
        assert((UINT64)end_loop_invocation == Uint64FromString(line));

        getline(loop_file, line);
        end_loop_iteration = Uint32FromString(line);
        assert((UINT64)end_loop_iteration == Uint64FromString(line));
    }

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

    cerr << start_loop << " " << start_loop_invocation << " " << start_loop_iteration << endl;
    cerr << end_loop << " " << end_loop_invocation << " " << end_loop_iteration << endl;
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

static UINT32 getSignalAddress(ADDRINT ssID);

static BOOL firstLoop = true;
static BOOL first_invocation = true;
static THREADID thread_started_invocation = -1;

static BOOL reached_start_invocation = false;
static BOOL reached_start_iteration = false;
static BOOL reached_end_invocation = false;
static BOOL reached_end_iteration = false;

static BOOL simulating_parallel_loop = false;
static BOOL ran_parallel_loop = false;
static BOOL start_next_parallel_loop = false;
static BOOL end_next_parallel_loop = false;

/* =========================================================================== */
static VOID checkEndLoop(ADDRINT loop)
{
  if(end_loop.length() > 0 && strncmp(end_loop.c_str(), (CHAR*)loop, 512) != 0) {
    return;
  }
  if (invocation_counts[loop] != end_loop_invocation) {
    return;
  }    
  cerr << "SETTING END INVOCATIION: " << (CHAR*)loop << endl;
  reached_end_invocation = true;
}

VOID ILDJIT_startLoop(THREADID tid, ADDRINT ip, ADDRINT loop)
{
  // This is for when there are serial loops within an executing parallel loop.
  if (simulating_parallel_loop) {
    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = true;
    
    if (tstate->pc_queue_valid) {
        // push Arg1 to stack
        tstate->ignore_list[tstate->get_queued_pc(1)] = true;
        // Call instruction to startLoop
        tstate->ignore_list[tstate->get_queued_pc(0)] = true;
    }
    lk_unlock(&tstate->lock);
  }

  if( (!ran_parallel_loop) && (reached_start_invocation) && (!reached_start_iteration) && (!start_next_parallel_loop) ) {
    start_next_parallel_loop = true;
    cerr << "SETTING MISSED START ITERATION:" << endl;
    cerr << (CHAR*)loop << endl;
  }

  if( (!ran_parallel_loop) && (reached_end_invocation) && (!reached_end_iteration) && (!end_next_parallel_loop)) {
    end_next_parallel_loop = true;
    //    reached_end_iteration = true;
    //    cerr << "SETTING REACHED END ITERATION B:" << endl;
    //    cerr << (CHAR*)loop << endl;
  }

  ran_parallel_loop = false;

  if(invocation_counts.count(loop) == 0) {
    invocation_counts[loop] = 0;
  }
  else {
    invocation_counts[loop]++;
  }

  // If we already started the phase invocation, 
  // check for end invocation, then return
  if(reached_start_invocation) {   
    checkEndLoop(loop);
    return;
  }
  
  if(start_loop.length() > 0 && strncmp(start_loop.c_str(), (CHAR*)loop, 512) != 0) {
    return;
  }

  // If the start loop hasn't got to the correct invocation yet, return
  if (invocation_counts[loop] != start_loop_invocation) {
    return;
  }
  assert(invocation_counts[loop] == start_loop_invocation);
  cerr << "Called startLoop() for the start invocation!:" << (CHAR*)loop << endl;
  reached_start_invocation = true;
  checkEndLoop(loop);
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
  disable_consumers();

  // create new loop state
  loop_states.push(loop_state_t());
  loop_state = &(loop_states.top());
    
  loop_state->simmed_iteration_count = 0;
  loop_state->current_loop = loop;

  ran_parallel_loop = true;

  if(end_next_parallel_loop) {
    reached_end_iteration = true;
  }
  
    /* Haven't started simulation and we encounter a loop we don't care
     * about */
    if (ExecMode != EXECUTION_MODE_SIMULATE) {
      if(!reached_start_invocation) {
        return;
      }
      cerr << "Do late!" << endl;
      doLateILDJITInstrumentation();
      cerr << "Done late!" << endl;
    }

    loop_state->use_ring_cache = (rc > 0);

    if(disable_wait_signal) {
      loop_state->use_ring_cache = false;
    }

    //#ifdef ZESTO_PIN_DBG
    CHAR* loop_name = (CHAR*) loop;
    cerr << "Starting loop: " << loop_name << "[" << invocation_counts[loop] << "]" << endl;
    //#endif

    list<THREADID>::iterator it;
    ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
        thread_state_t* curr_tstate = get_tls(*it);
        lk_lock(&curr_tstate->lock, tid+1);
        /* This is called right before a wait spin loop.
         * For this thread only, ignore the end of the wait, so we
         * can actually start simulating and unblock everyone else.
         */
        curr_tstate->unmatchedWaits = 0;
        lk_unlock(&curr_tstate->lock);
    }

    iteration_counts[loop_state->current_loop] = 0;    
    
    if(firstLoop) {
      if(start_loop.size() > 0) {
        cerr << "FastForward runtime:";
        printElapsedTime();
      }
      cerr << "Starting simulation, TID: " << tid << endl;
      PPointHandler(CONTROL_START, NULL, NULL, (VOID*)ip, tid);
      firstLoop = false;
    }
    else {
      cerr << tid << ": resuming simulation" << endl;
      ILDJIT_ResumeSimulation(tid);
    }
    simulating_parallel_loop = true;
}
/* ========================================================================== */

// Assumes that start iteration calls _always_ happen in sequential order, 
// in a thread safe manner!  Have to take Simon's word on this one for now...
// Must be called from within the body of MOLECOOL_beforeWait!
VOID ILDJIT_startIteration(THREADID tid)
{
  UINT32 current_iteration = iteration_counts[loop_state->current_loop];
  iteration_counts[loop_state->current_loop]++;

  if(reached_start_invocation && reached_start_iteration) {
    loop_state->simmed_iteration_count++;
    
    if(reached_end_iteration) {
      ILDJIT_PauseSimulation(tid);
      cerr << "Simulation runtime:";
      printElapsedTime();
      cerr << "LStopping simulation, TID: " << tid << endl;
      StopSimulation(true);
      cerr << "[KEVIN] Stopped simulation! " << tid << endl;
      return;
    }
      
    if(reached_end_invocation && (current_iteration == end_loop_iteration)) {
      cerr << "SETTING REACHED END ITERATION A" << endl;
      reached_end_iteration = true;
    }
    return;
  }
  
  if(!reached_start_invocation) {
    return;
  }

  if( (current_iteration == start_loop_iteration) || (start_next_parallel_loop && ran_parallel_loop)) {
    reached_start_iteration = true;
    loop_state->simmed_iteration_count = 1;
    thread_started_invocation = tid;
    start_next_parallel_loop = false;
  }
  
  if(reached_end_invocation && (current_iteration == end_loop_iteration)) {
    cerr << "SETTING REACHED END ITERATION B" << endl;
    reached_end_iteration = true;
  }
}

/* ========================================================================== */
VOID ILDJIT_endParallelLoop(THREADID tid, ADDRINT loop, ADDRINT numIterations)
{
#ifdef ZESTO_PIN_DBG
    cerr << tid << ": Pausing simulation!" << endl;
#endif

    if (ExecMode == EXECUTION_MODE_SIMULATE) {
        ILDJIT_PauseSimulation(tid);
        cerr << tid << ": Paused simulation!" << endl;
        first_invocation = false;

        list<THREADID>::iterator it;
        ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
            thread_state_t* tstate = get_tls(*it);
            lk_lock(&tstate->lock, tid+1);
            tstate->ignore = true;
            lk_unlock(&tstate->lock);
        }
//#ifdef ZESTO_PIN_DBG
        CHAR* loop_name = (CHAR*) loop;
        cerr << "Ending loop: " << loop_name << " NumIterations:" << (UINT32)numIterations << endl;
//#endif
    }
    simulating_parallel_loop = false;
    loop_states.pop();
    loop_state = &(loop_states.top());
}

/* ========================================================================== */
VOID ILDJIT_beforeWait(THREADID tid, ADDRINT ssID_addr, ADDRINT ssID, ADDRINT pc)
{
#ifdef PRINT_WAITS
    if (ExecMode == EXECUTION_MODE_SIMULATE)
        cerr << tid <<" :Before Wait "<< hex << pc << dec  << " ID: " << ssID << hex << " (" << ssID_addr <<")" << dec << endl;
#endif

    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = true;

    if (tstate->pc_queue_valid)
    {
        // push Arg2 to stack
        tstate->ignore_list[tstate->get_queued_pc(2)] = true;
        //        cerr << tid << ": Ignoring instruction at pc: " << hex << tstate->get_queued_pc(2) << dec << endl;

        // push Arg1 to stack
        tstate->ignore_list[tstate->get_queued_pc(1)] = true;
        //        cerr << tid << ": Ignoring instruction at pc: " << hex << tstate->get_queued_pc(1) << dec << endl;

        // Call instruction to beforeWait
        tstate->ignore_list[tstate->get_queued_pc(0)] = true;
        //        cerr << tid << ": Ignoring instruction at pc: " << hex << tstate->get_queued_pc(0) << dec << endl;
    }
    lk_unlock(&tstate->lock);

    if(num_threads == 1) {
        return;
    }

    tstate->lastWaitID = ssID;
}

/* ========================================================================== */
VOID ILDJIT_afterWait(THREADID tid, ADDRINT ssID, ADDRINT is_light, ADDRINT pc)
{
    thread_state_t* tstate = get_tls(tid);
    handshake_container_t* handshake;

    // Assumes all startIteration calls are sequential and within MOLECOOL_beforeWait()
    // don't need lock
    if(!reached_start_iteration) {
      return;
    }

    lk_lock(&tstate->lock, tid+1);       
    
    tstate->ignore = false;

#ifdef PRINT_WAITS
    if (ExecMode == EXECUTION_MODE_SIMULATE)
        cerr << tid <<": After Wait "<< hex << pc << dec  << " ID: " << tstate->lastWaitID << endl;
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

    tstate->unmatchedWaits++;

    /* Ignore injecting waits until the end of the first iteration,
     * so we can start simulating */
    assert(loop_state->simmed_iteration_count > 0);
    if (loop_state->simmed_iteration_count == 1)
        goto cleanup;

    if(!(loop_state->use_ring_cache)) {
      goto cleanup;
    }

    if(is_light) {
        goto cleanup;
    }

    /* Insert wait instruction in pipeline */
    handshake = handshake_buffer.get_buffer(tid);

    handshake->flags.isFirstInsn = false;
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
//    ASSERTX(tstate->lastSignalAddr != 0xdecafbad);
    *(INT32*)(&handshake->handshake.ins[1]) = getSignalAddress(ssID); 
//    cerr << tid << ": Vodoo load instruction " << hex << pc <<  " ID: " << tstate->lastSignalAddr << dec << endl;
    handshake_buffer.producer_done(tid);

cleanup:
    tstate->lastSignalAddr = 0xdecafbad;
}

/* ========================================================================== */
VOID ILDJIT_beforeSignal(THREADID tid, ADDRINT ssID_addr, ADDRINT ssID, ADDRINT pc)
{
    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = true;

#ifdef PRINT_WAITS
    if (ExecMode == EXECUTION_MODE_SIMULATE)
        cerr << tid <<": Before Signal " << hex << pc << " ID: " << ssID <<  " (" << ssID_addr << ")" << dec << endl;
#endif

    if (tstate->pc_queue_valid)
    {
        // push Arg2 to stack
        tstate->ignore_list[tstate->get_queued_pc(2)] = true;
        //        cerr << tid << ": Ignoring instruction at pc: " << hex << tstate->get_queued_pc(2) << dec << endl;

        // push Arg1 to stack
        tstate->ignore_list[tstate->get_queued_pc(1)] = true;
        //        cerr << tid << ": Ignoring instruction at pc: " << hex << tstate->get_queued_pc(1) << dec << endl;

        // Call instruction to beforeWait
        tstate->ignore_list[tstate->get_queued_pc(0)] = true;
        //        cerr << tid << ": Ignoring instruction at pc: " << hex << tstate->get_queued_pc(0) << dec << endl;
    }
    lk_unlock(&tstate->lock);

//    ASSERTX(tstate->lastSignalAddr == 0xdecafbad);
}

/* ========================================================================== */
VOID ILDJIT_afterSignal(THREADID tid, ADDRINT ssID_addr, ADDRINT ssID, ADDRINT pc)
{
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
    if (ExecMode == EXECUTION_MODE_SIMULATE)
        cerr << tid <<": After Signal " << hex << pc << dec << endl;
#endif

    /* Not simulating -- just ignore. */
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        goto cleanup;

    /* Don't insert signals in single-core mode */
    if (num_threads == 1)
        goto cleanup;

    tstate->unmatchedWaits--;
    ASSERTX(tstate->unmatchedWaits >= 0);

    if(!(loop_state->use_ring_cache)) {
      return;
    }

    /* Insert signal instruction in pipeline */
    handshake = handshake_buffer.get_buffer(tid);

    handshake->flags.isFirstInsn = false;
    handshake->handshake.sleep_thread = false;
    handshake->handshake.resume_thread = false;
    handshake->handshake.real = false;
    handshake->handshake.in_critical_section = (num_threads > 1) && (tstate->unmatchedWaits > 0);
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
    handshake_buffer.producer_done(tid);

cleanup:
    tstate->lastSignalAddr = 0xdecafbad;
}

/* ========================================================================== */
VOID ILDJIT_setAffinity(THREADID tid, INT32 coreID)
{
    ASSERTX(coreID >= 0 && coreID < num_threads);
    HardcodeSchedule(tid, coreID);
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
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_beforeWait),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_INST_PTR,
                       IARG_CALL_ORDER, CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_afterWait");
    if (RTN_Valid(rtn))
    {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(ILDJIT_afterWait),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
                       IARG_INST_PTR,
                       IARG_CALL_ORDER, CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_beforeSignal");
    if (RTN_Valid(rtn))
    {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_beforeSignal),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_INST_PTR,
                       IARG_CALL_ORDER, CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_afterSignal");
    if (RTN_Valid(rtn))
    {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(ILDJIT_afterSignal),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_INST_PTR,
                       IARG_CALL_ORDER, CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_endParallelLoop");
    if (RTN_Valid(rtn))
    {
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
    thread_state_t *tstate = get_tls(thread_started_invocation);
    firstCore = tstate->coreID;
  }
  assert(firstCore < 256);
  assert(ssID < 256);

  return 0x7fff0000 + (firstCore << 8) + ssID;
  //  return 0x7fff0000 + (firstCore << 9) + ssID;
}

/*VOID doLateCallbacks()
{
  PIN_LockClient();
  //  for( IMG img= APP_ImgHead(); IMG_Valid(img); img = IMG_Next(img) ) {
  //    RTN rtn;
  
  CODECACHE_FlushCache();
  PIN_UnlockClient();

}
*/
/* ========================================================================== */
VOID ILDJIT_PauseSimulation(THREADID tid)
{
    /* The context is that all cores functionally have sent signal 0
     * and unblocked the last iteration. We need to (i) wait for them
     * to functionally reach wait 0, where they will wait until the end
     * of the loop; (ii) drain all pipelines once cores are waiting. */
    volatile bool done_with_iteration = false;
    do {
        done_with_iteration = true;
        list<THREADID>::iterator it;
        ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
            if ((*it) != tid) {
                thread_state_t* tstate = get_tls(*it);
                lk_lock(&tstate->lock, tid + 1);
                bool curr_done = tstate->ignore && (tstate->lastWaitID == 0);
                done_with_iteration &= curr_done;
                /* Setting ignore_all here (while ignore is set) should be a race-free way
                 * of ignoring the serial portion outside the loop after the thread goes
                 * on an unsets ignore locally. */
                if (curr_done) {
                    tstate->ignore_all = true;
                    tstate->sleep_producer = true;
                }
                lk_unlock(&tstate->lock);
            }
        }
    } while (!done_with_iteration);

    /* Here we have produced everything for this loop! */
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
        if (!done)
            PIN_Sleep(1000);
//            PIN_Yield();
    } while (!done);

#ifdef ZESTO_PIN_DBG
    cerr << tid << " [" << sim_cycle << ":KEVIN]: All cores have empty buffers" << endl;
    cerr.flush();
#endif

    /* Have thread ignore serial section after */
    /* XXX: Do we need this? A few lines above we set ignore_all! */
    ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
        thread_state_t* tstate = get_tls(*it);
        lk_lock(&tstate->lock, *it+1);
        handshake_buffer.resetPool(*it);
        tstate->ignore = true;
        tstate->sleep_producer = false;
        lk_unlock(&tstate->lock);
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
        activate_core(coreID);
        thread_state_t* tstate = get_tls(curr_tid);
        lk_lock(&tstate->lock, tid+1);
        tstate->ignore_all = false;
        lk_unlock(&tstate->lock);
    }
}
