/*
 * ILDJIT-specific functions for zesto feeder
 * Copyright, Svilen Kanev, 2012
 */

#include <stdlib.h>
#include <unistd.h>
#include <map>
#include <signal.h>
#include <queue>
#include <signal.h>
#include "feeder.h"
#include "ildjit.h"
#include "fluffy.h"
#include "BufferManager.h"

extern tick_t sim_cycle;
extern BufferManager handshake_buffer;

// True if ILDJIT has finished compilation and is executing user code
BOOL ILDJIT_executionStarted = false;

// True if a non-compiler thread is being created
BOOL ILDJIT_executorCreation = false;

XIOSIM_LOCK ildjit_lock;

const UINT8 ld_template[] = {0xa1, 0xad, 0xfb, 0xca, 0xde};
const UINT8 st_template[] = {0xc7, 0x05, 0xad, 0xfb, 0xca, 0xde, 0x00, 0x00, 0x00, 0x00 };

static map<ADDRINT, INT32> invocation_counts;

KNOB<string> KnobLoopIDFile(KNOB_MODE_WRITEONCE, "pintool",
    "loopID", "", "File to get start/end loop IDs and invocation caounts");

KNOB<BOOL> KnobDisableWaitSignal(KNOB_MODE_WRITEONCE,     "pintool",
        "disable_wait_signal", "false", "Don't insert any waits or signals into the pipeline");

static string start_loop = "";
static UINT64 start_loop_invocation = -1;
static UINT64 start_loop_iteration = -1;

static string end_loop = "";
static UINT64 end_loop_invocation = -1;
static UINT64 end_loop_iteration = -1;

VOID printMemoryUsage(THREADID tid);
VOID printElapsedTime();
VOID AddILDJITWaitSignalCallbacks();
BOOL signalCallback(THREADID tid, INT32 sig, CONTEXT *ctxt, BOOL hasHandler, const EXCEPTION_INFO *pExceptInfo, VOID *v);
void signalCallback2(int signum);

extern VOID doLateILDJITInstrumentation();

time_t last_time;

bool use_ring_cache;
bool disable_wait_signal;

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
	start_loop_invocation = Uint64FromString(line);
	
	getline(loop_file, line);
	start_loop_iteration = Uint64FromString(line);

	// end phase info
	getline(loop_file, end_loop);

	getline(loop_file, line);
	end_loop_invocation = Uint64FromString(line);
	
	getline(loop_file, line);
	end_loop_iteration = Uint64FromString(line);
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

    cerr << start_loop << " " << start_loop_invocation << endl;
    cerr << end_loop << " " << end_loop_invocation << endl;
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
VOID ILDJIT_endSimulation(THREADID tid, ADDRINT ip)
{
    if (strlen(end_loop) != 0)
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

static BOOL firstLoop = true;
static BOOL seen_ssID_zero = false;
static BOOL seen_ssID_zero_twice = false;
/* =========================================================================== */
VOID ILDJIT_startLoop(THREADID tid, ADDRINT ip, ADDRINT loop)
{
  invocation_counts[loop]++;
}

/* ========================================================================== */
VOID ILDJIT_startParallelLoop(THREADID tid, ADDRINT ip, ADDRINT loop, ADDRINT rc)
{
    /* Haven't started simulation and we encounter a loop we don't care
     * about */
    if (ExecMode != EXECUTION_MODE_SIMULATE) {
        if(strlen(start_loop) > 0 && strncmp(start_loop, (CHAR*)loop, 512) != 0) {
            return;
        }
        if(invocation_counts[loop] < start_loop_invocation) {
            return;
        }
        if(invocation_counts[loop] == start_loop_invocation) {
            doLateILDJITInstrumentation();
        }
    }

    use_ring_cache = (rc > 0);

    if(disable_wait_signal) {
        use_ring_cache = false;
    }

//#ifdef ZESTO_PIN_DBG
    CHAR* loop_name = (CHAR*) loop;
    cerr << "Starting loop: " << loop_name << "[" << invocation_counts[loop] << "]" << endl;
//#endif
    vector<THREADID>::iterator it;
    for (it = thread_list.begin(); it != thread_list.end(); it++) {
        thread_state_t* curr_tstate = get_tls(*it);
        lk_lock(&curr_tstate->lock, tid+1);
        /* This is called right before a wait spin loop.
         * For this thread only, ignore the end of the wait, so we
         * can actually start simulating and unblock everyone else.
         */
        if (*it == tid)
            curr_tstate->firstIteration = true;
        curr_tstate->unmatchedWaits = 0;
        curr_tstate->afterSignalCount = 0;
        curr_tstate->afterWaitLightCount = 0;
        curr_tstate->afterWaitHeavyCount = 0;
        curr_tstate->ignored_before_wait = false;
        curr_tstate->ignored_before_signal = false;
        lk_unlock(&curr_tstate->lock);
    }

    thread_state_t* core0_tstate = get_tls(core_threads[0]);
    lk_lock(&core0_tstate->lock, tid+1);
    seen_ssID_zero = false;
    seen_ssID_zero_twice = false;
    lk_unlock(&core0_tstate->lock);

    if (strlen(start_loop) == 0 && firstLoop) {
        cerr << "Starting simulation, TID: " << tid << endl;
        PPointHandler(CONTROL_START, NULL, NULL, (VOID*)ip, tid);
        firstLoop = false;
    }
    else if (strncmp(start_loop, (CHAR*)loop, 512) == 0) {
        if (invocation_counts[loop] == start_loop_invocation) {
            cerr << "FastForward runtime:";
            printElapsedTime();

            cerr << "Starting simulation, TTID: " << tid << endl;
            PPointHandler(CONTROL_START, NULL, NULL, (VOID*)ip, tid);
            cerr << "Starting simulation, TTID2: " << tid << endl;
            firstLoop = false;
        }
        else if (invocation_counts[loop] > start_loop_invocation) {
            cerr << tid << ": resuming simulation" << endl;
            ResumeSimulation(tid);
        }
    }
    else {
        cerr << tid << ": resuming simulation" << endl;
        ResumeSimulation(tid);
    }
}
/* ========================================================================== */

VOID ILDJIT_endParallelLoop(THREADID tid, ADDRINT loop, ADDRINT numIterations)
{
    if (ExecMode == EXECUTION_MODE_SIMULATE) {
        PauseSimulation(tid);
        cerr << tid << ": Paused simulation!" << endl;

        vector<THREADID>::iterator it;
        for (it = thread_list.begin(); it != thread_list.end(); it++) {
            thread_state_t* tstate = get_tls(*it);
            lk_lock(&tstate->lock, tid+1);
            tstate->ignore = true;
            tstate->afterSignalCount = 0;
            tstate->afterWaitLightCount = 0;
            tstate->afterWaitHeavyCount = 0;
            lk_unlock(&tstate->lock);
        }
//#ifdef ZESTO_PIN_DBG
        CHAR* loop_name = (CHAR*) loop;
        cerr << "Ending loop: " << loop_name << " NumIterations:" << (UINT32)numIterations << endl;
//#endif
    }

    if(strncmp(end_loop, (CHAR*)loop, 512) == 0 && invocation_counts[loop] == end_loop_invocation) {
        cerr << "Simulation runtime:";
        printElapsedTime();
        cerr << "LStopping simulation, TID: " << tid << endl;
        StopSimulation(tid);
        cerr << "[KEVIN] Stopped simulation! " << tid << endl;
    }
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

    if (tstate->pc_queue_valid && !tstate->ignored_before_wait)
    {
        // push Arg3 to stack
        tstate->ignore_list[tstate->get_queued_pc(3)] = true;

        // push Arg2 to stack
        tstate->ignore_list[tstate->get_queued_pc(2)] = true;
        //        cerr << tid << ": Ignoring instruction at pc: " << hex << tstate->get_queued_pc(2) << dec << endl;

        // push Arg1 to stack
        tstate->ignore_list[tstate->get_queued_pc(1)] = true;
        //        cerr << tid << ": Ignoring instruction at pc: " << hex << tstate->get_queued_pc(1) << dec << endl;

        // Call instruction to beforeWait
        tstate->ignore_list[tstate->get_queued_pc(0)] = true;
        //        cerr << tid << ": Ignoring instruction at pc: " << hex << tstate->get_queued_pc(0) << dec << endl;

        tstate->ignored_before_wait = true;
    }
    lk_unlock(&tstate->lock);

    if(num_threads == 1) {
        return;
    }

//    ASSERTX(tstate->lastSignalAddr == 0xdecafbad);
    tstate->lastSignalAddr = 0x7fff0000 + ssID;
    tstate->lastWaitID = ssID;

    // XXX: HACKEDY HACKEDY HACK The ordering of these conditions matters
    if ((ExecMode == EXECUTION_MODE_SIMULATE) && (core_threads[0] != tid)) {
        tstate->firstIteration = false;
    }

    if (core_threads[0] == tid) {
        lk_lock(&tstate->lock, tid+1);
        if ((ssID == 0) && (ExecMode == EXECUTION_MODE_SIMULATE) && seen_ssID_zero) {
            seen_ssID_zero_twice = true;
        }

        if ((ExecMode == EXECUTION_MODE_SIMULATE) && seen_ssID_zero_twice) {
            tstate->firstIteration = false;
        }

        if ((ssID == 0) && (ExecMode == EXECUTION_MODE_SIMULATE)) {
            seen_ssID_zero = true;
        }
        lk_unlock(&tstate->lock);
    }
}

/* ========================================================================== */
VOID ILDJIT_afterWait(THREADID tid, ADDRINT is_light, ADDRINT pc)
{
    thread_state_t* tstate = get_tls(tid);
    handshake_container_t* handshake;

    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = false;

#ifdef PRINT_WAITS
    if (ExecMode == EXECUTION_MODE_SIMULATE)
        cerr << tid <<": After Wait "<< hex << pc << dec  << " ID: " << tstate->lastWaitID << endl;
#endif

    // Indicates not in a wait any more
    tstate->lastWaitID = -1;

    /* Not simulatiing -- just ignore. */
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
    if (tstate->firstIteration)
        goto cleanup;

    if(!use_ring_cache)
        goto cleanup;

    if(is_light) {
        tstate->afterWaitLightCount++;
        goto cleanup;
    }

    tstate->afterWaitHeavyCount++;

    /* Insert wait instruction in pipeline */
    handshake = handshake_buffer.get_buffer(tid);

    handshake->flags.isFirstInsn = false;
    handshake->handshake.sleep_thread = false;
    handshake->handshake.resume_thread = false;
    handshake->handshake.real = false;
    handshake->handshake.coreID = tstate->coreID;
    handshake->handshake.iteration_correction = false;
    handshake->handshake.in_critical_section = (num_threads > 1);
    handshake->flags.valid = true;

    handshake->handshake.pc = pc;
    handshake->handshake.npc = pc + sizeof(ld_template);
    handshake->handshake.tpc = pc + sizeof(ld_template);
    handshake->handshake.brtaken = false;
    memcpy(handshake->handshake.ins, ld_template, sizeof(ld_template));
    // Address comes right after opcode byte
//    ASSERTX(tstate->lastSignalAddr != 0xdecafbad);
    *(INT32*)(&handshake->handshake.ins[1]) = tstate->lastSignalAddr;
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

    if (tstate->pc_queue_valid && !tstate->ignored_before_signal)
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

        tstate->ignored_before_signal = true;
    }
    lk_unlock(&tstate->lock);

//    ASSERTX(tstate->lastSignalAddr == 0xdecafbad);
    tstate->lastSignalAddr = 0x7fff0000 + ssID;
}

/* ========================================================================== */
VOID ILDJIT_afterSignal(THREADID tid, ADDRINT ssID_addr, ADDRINT ssID, ADDRINT pc)
{
    thread_state_t* tstate = get_tls(tid);
    handshake_container_t* handshake;

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

    if(!use_ring_cache) {
        return;
    }

    tstate->afterSignalCount++;

    /* Insert signal instruction in pipeline */
    handshake = handshake_buffer.get_buffer(tid);

    handshake->flags.isFirstInsn = false;
    handshake->handshake.sleep_thread = false;
    handshake->handshake.resume_thread = false;
    handshake->handshake.real = false;
    handshake->handshake.coreID = tstate->coreID;
    handshake->handshake.in_critical_section = (num_threads > 1) && (tstate->unmatchedWaits > 0);
    handshake->handshake.iteration_correction = false;
    handshake->flags.valid = true;

    handshake->handshake.pc = pc;
    handshake->handshake.npc = pc + sizeof(st_template);
    handshake->handshake.tpc = pc + sizeof(st_template);
    handshake->handshake.brtaken = false;
    memcpy(handshake->handshake.ins, st_template, sizeof(st_template));
    // Address comes right after opcode and MoodRM bytes
//    ASSERTX(tstate->lastSignalAddr != 0xdecafbad);
    *(INT32*)(&handshake->handshake.ins[2]) = tstate->lastSignalAddr;

//    cerr << tid << ": Vodoo store instruction " << hex << pc << " ID: " << tstate->lastSignalAddr << dec << endl;
    handshake_buffer.producer_done(tid);

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
    return;
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
}
