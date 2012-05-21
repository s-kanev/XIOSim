/* 
 * ILDJIT-specific functions for zesto feeder 
 * Copyright, Svilen Kanev, 2012
 */

#include <map>
#include <queue>
#include "feeder.h"
#include "ildjit.h"
#include "fluffy.h"

// True if ILDJIT has finished compilation and is executing user code
BOOL ILDJIT_executionStarted = false;

// True if a non-compiler thread is being created
BOOL ILDJIT_executorCreation = false;

PIN_LOCK ildjit_lock;

const UINT8 ld_template[] = {0xa1, 0xad, 0xfb, 0xca, 0xde};
const UINT8 st_template[] = {0xc7, 0x05, 0xad, 0xfb, 0xca, 0xde, 0x00, 0x00, 0x00, 0x00 };

map<THREADID, INT32> lastWaitID;

// has this thread already ignored call overhead for before_wait
static map<THREADID, bool> ignored_before_wait;
// has this thread already ignored call overhead for before_signal
static map<THREADID, bool> ignored_before_signal;

static map<ADDRINT, INT32> invocation_counts;

KNOB<string> KnobLoopIDFile(KNOB_MODE_WRITEONCE, "pintool",
    "loopID", "", "File to get start/end loop IDs and invocation caounts");
static CHAR start_loop[512] = {0};
static INT32 start_loop_invocation;
static CHAR end_loop[512] = {0};
static INT32 end_loop_invocation;

static map<THREADID, INT32> unmatchedWaits;

/* ========================================================================== */
VOID MOLECOOL_Init()
{
    InitLock(&ildjit_lock);

    FLUFFY_Init();

    if (!KnobLoopIDFile.Value().empty()) {
        ifstream loop_file;
        loop_file.open(KnobLoopIDFile.Value().c_str(), ifstream::in);
        if (loop_file.fail()) {
            cerr << "Couldn't open loop id file: " << KnobLoopIDFile.Value() << endl;
            PIN_ExitProcess(1);
        }
        loop_file.getline(start_loop, 512);
        loop_file >> start_loop_invocation;
        loop_file.get();
        loop_file.getline(end_loop, 512);
        loop_file >> end_loop_invocation;
    }
    cerr << start_loop << " " << start_loop_invocation << endl;
    cerr << end_loop << " " << end_loop_invocation << endl;
}

/* ========================================================================== */
BOOL ILDJIT_IsExecuting()
{
    bool res;
    GetLock(&ildjit_lock, 1);
    res = ILDJIT_executionStarted;
    ReleaseLock(&ildjit_lock);
    return res;
}

/* ========================================================================== */
BOOL ILDJIT_IsCreatingExecutor()
{
    bool res;
    GetLock(&ildjit_lock, 1);
    res = ILDJIT_executorCreation;
    ReleaseLock(&ildjit_lock);
    return res;
}

/* ========================================================================== */
VOID ILDJIT_startSimulation(THREADID tid, ADDRINT ip)
{
    GetLock(&ildjit_lock, 1);

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

    /* This thread gets core 0 by convention. HELIX takes care of
     * setting the rest of the core IDs. */
    thread_state_t* tstate = get_tls(tid);
    tstate->coreID = 0;
    core_threads[0] = tid;
    cerr << tid << ": assigned to core " << tstate->coreID << endl;

    ReleaseLock(&ildjit_lock);
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
    GetLock(&ildjit_lock, 1);

    ILDJIT_executorCreation = true;

//#ifdef ZESTO_PIN_DBG
    cerr << "Starting creation, TID: " << tid << endl;
//#endif

    ReleaseLock(&ildjit_lock);
}

/* ========================================================================== */
VOID ILDJIT_ExecutorCreateEnd(THREADID tid)
{
    //Dummy, actual work now done in ILDJIT_ThreadStarting
}

static BOOL firstLoop = true;
static BOOL seen_ssID_zero = false;

/* ========================================================================== */
VOID ILDJIT_startParallelLoop(THREADID tid, ADDRINT ip, ADDRINT loop)
{
#ifdef ZESTO_PIN_DBG
    CHAR* loop_name = (CHAR*) loop;
    cerr << "Starting loop: " << loop_name << endl;
#endif
    invocation_counts[loop]++;

    /* Haven't started simulation and we encounter a loop we don't care
     * about */
    if (ExecMode != EXECUTION_MODE_SIMULATE &&
        strncmp(start_loop, (CHAR*)loop, 512) != 0)
        return;

    /* This is called right before a wait spin loop.
     * For this thread only, ignore the end of the wait, so we
     * can actually start simulating and unblock everyone else.
     */
    thread_state_t* tstate = get_tls(tid);
    tstate->firstIteration = true;
    map<THREADID, handshake_queue_t>::iterator it;
    for (it = handshake_buffer.begin(); it != handshake_buffer.end(); it++)
        unmatchedWaits[tid] = 0;

    ignored_before_wait.clear();
    ignored_before_signal.clear();
    seen_ssID_zero = false;

    if ((strlen(start_loop) == 0 && firstLoop) ||
        (strncmp(start_loop, (CHAR*)loop, 512) == 0 && invocation_counts[loop] == start_loop_invocation)) {
        cerr << "Starting simulation, TID: " << tid << endl;
        if (KnobFluffy.Value().empty())
            PPointHandler(CONTROL_START, NULL, NULL, (VOID*)ip, tid);
        firstLoop = false;
    } else {
        cerr << tid << ": resuming simulation" << endl;
        ResumeSimulation(tid);
    }
}

/* ========================================================================== */
VOID ILDJIT_endParallelLoop(THREADID tid, ADDRINT loop)
{
#ifdef ZESTO_PIN_DBG
    CHAR* loop_name = (CHAR*) loop;
    cerr << "Ending loop: " << loop_name << endl;
#endif

    if (ExecMode == EXECUTION_MODE_SIMULATE) {
        cerr << tid << ": Pausing simulation" << endl;
        PauseSimulation(tid);
    }

    if(strncmp(end_loop, (CHAR*)loop, 512) == 0 && invocation_counts[loop] == end_loop_invocation) {
        cerr << "LStopping simulation, TID: " << tid << endl;
        StopSimulation(tid);
    }
}

/* ========================================================================== */
VOID ILDJIT_beforeWait(THREADID tid, ADDRINT ssID_addr, ADDRINT ssID, ADDRINT pc)
{
    GetLock(&simbuffer_lock, tid+1);
//    ignore[tid] = true;
//    cerr << tid <<": Before Wait " << hex << ssID << dec << endl;

    thread_state_t* tstate = get_tls(tid);
    if (tstate->pc_queue_valid &&
        ignored_before_wait.find(tid) == ignored_before_wait.end())
    {
        // push Arg2 to stack
        ignore_list[tid][tstate->get_queued_pc(2)] = true;
        cerr << tid << ": Ignoring instruction at pc: " << hex << tstate->get_queued_pc(2) << dec << endl;

        // push Arg1 to stack
        ignore_list[tid][tstate->get_queued_pc(1)] = true;
        cerr << tid << ": Ignoring instruction at pc: " << hex << tstate->get_queued_pc(1) << dec << endl;

        // Call instruction to beforeWait
        ignore_list[tid][tstate->get_queued_pc(0)] = true;
        cerr << tid << ": Ignoring instruction at pc: " << hex << tstate->get_queued_pc(0) << dec << endl;

        ignored_before_wait[tid] = true;
    }

    while(inserted_pool[tid].empty())
    {
        ReleaseLock(&simbuffer_lock);
        PIN_Yield();
        GetLock(&simbuffer_lock, tid+1);
    }
    handshake_container_t* handshake = inserted_pool[tid].front();

    handshake->isFirstInsn = false;
    handshake->handshake.sleep_thread = true;
    handshake->handshake.resume_thread = false;
    handshake->handshake.real = false;
    handshake->handshake.pc = 0;
    handshake->handshake.coreID = tstate->coreID;
    handshake->handshake.iteration_correction = (ssID == 0);
    handshake->valid = true;

    tstate->lastSignalAddr = ssID_addr;
    lastWaitID[tid] = ssID;

    if ((ssID == 0) && (ExecMode == EXECUTION_MODE_SIMULATE) &&
        seen_ssID_zero)
        tstate->firstIteration = false;

    if ((ssID == 0) && (ExecMode == EXECUTION_MODE_SIMULATE))
        seen_ssID_zero = true;

    handshake_buffer[tid].push(handshake);
    inserted_pool[tid].pop();
    ReleaseLock(&simbuffer_lock);
}

/* ========================================================================== */
VOID ILDJIT_afterWait(THREADID tid, ADDRINT pc)
{
    GetLock(&simbuffer_lock, tid+1);
//    ignore[tid] = false;
//    cerr << tid <<": After Wait "<< hex << pc << dec  << " ID: " << lastWaitID[tid] << endl;

    /* HACKEDY HACKEDY HACK */
    /* We are not simulating and the core still hasn't consumed the wait.
     * Find the dummy handshake in the simulation queue and remove it. */
    if (ExecMode != EXECUTION_MODE_SIMULATE || ignore_all)
    {
        if (!handshake_buffer[tid].empty())
        {
            handshake_container_t* hshake = handshake_buffer[tid].front();
            ASSERTX(hshake->handshake.real == false);
            ASSERTX(hshake->handshake.sleep_thread == true);
            handshake_buffer[tid].pop();
            inserted_pool[tid].push(hshake);
        }
        ReleaseLock(&simbuffer_lock);
        return;
    }

    while(inserted_pool[tid].empty())
    {
        ReleaseLock(&simbuffer_lock);
        PIN_Yield();
        GetLock(&simbuffer_lock, tid+1);
    }
    handshake_container_t* handshake = inserted_pool[tid].front();

    thread_state_t* tstate = get_tls(tid);
    handshake->isFirstInsn = false;
    handshake->handshake.sleep_thread = false;
    handshake->handshake.resume_thread = true;
    handshake->handshake.real = false;
    handshake->handshake.pc = 0;
    handshake->handshake.coreID = tstate->coreID;
    handshake->handshake.in_critical_section = (num_threads > 1);
    handshake->handshake.iteration_correction = false;
    handshake->valid = true;

    unmatchedWaits[tid]++;

    handshake_buffer[tid].push(handshake);
    inserted_pool[tid].pop();

    /* Ignore injecting waits until the end of the first iteration,
     * so we can start simulating */
    if (tstate->firstIteration)
    {
        ReleaseLock(&simbuffer_lock);
        return;
    }

    /* Don't insert waits in single-core mode */
    if (num_threads < 2) {
        ReleaseLock(&simbuffer_lock);
        return;
    }

    /* Insert wait instruction in pipeline */
    while(inserted_pool[tid].empty())
    {
        ReleaseLock(&simbuffer_lock);
        PIN_Yield();
        GetLock(&simbuffer_lock, tid+1);
    }


    handshake = inserted_pool[tid].front();

    handshake->isFirstInsn = false;
    handshake->handshake.sleep_thread = false;
    handshake->handshake.resume_thread = false;
    handshake->handshake.real = false;
    handshake->handshake.coreID = tstate->coreID;
    handshake->handshake.iteration_correction = false;
    handshake->valid = true;

    handshake->handshake.pc = pc;
    handshake->handshake.npc = pc + sizeof(ld_template);
    handshake->handshake.tpc = pc + sizeof(ld_template);
    handshake->handshake.brtaken = false;
    memcpy(handshake->handshake.ins, ld_template, sizeof(ld_template));
    // Address comes right after opcode byte
    *(INT32*)(&handshake->handshake.ins[1]) = tstate->lastSignalAddr;

//    cerr << tid << ": Vodoo load instruction " << hex << pc <<  " ID: " << tstate->lastSignalAddr << dec << endl;
    handshake_buffer[tid].push(handshake);
    inserted_pool[tid].pop();

    ReleaseLock(&simbuffer_lock);
}

/* ========================================================================== */
VOID ILDJIT_beforeSignal(THREADID tid, ADDRINT ssID_addr, ADDRINT ssID, ADDRINT pc)
{
    GetLock(&simbuffer_lock, tid+1);
//    ignore[tid] = true;
//    cerr << tid <<": Before Signal " << hex << ssID << dec << endl;

    thread_state_t* tstate = get_tls(tid);
    if (tstate->pc_queue_valid &&
        ignored_before_signal.find(tid) == ignored_before_signal.end())
    {
        // push Arg2 to stack
        ignore_list[tid][tstate->get_queued_pc(2)] = true;
        cerr << tid << ": Ignoring instruction at pc: " << hex << tstate->get_queued_pc(2) << dec << endl;

        // push Arg1 to stack
        ignore_list[tid][tstate->get_queued_pc(1)] = true;
        cerr << tid << ": Ignoring instruction at pc: " << hex << tstate->get_queued_pc(1) << dec << endl;

        // Call instruction to beforeWait
        ignore_list[tid][tstate->get_queued_pc(0)] = true;
        cerr << tid << ": Ignoring instruction at pc: " << hex << tstate->get_queued_pc(0) << dec << endl;

        ignored_before_signal[tid] = true;
    }

    while(inserted_pool[tid].empty())
    {
        ReleaseLock(&simbuffer_lock);
        PIN_Yield();
        GetLock(&simbuffer_lock, tid+1);
    }
    handshake_container_t* handshake = inserted_pool[tid].front();

    handshake->isFirstInsn = false;
    handshake->handshake.sleep_thread = true;
    handshake->handshake.resume_thread = false;
    handshake->handshake.real = false;
    handshake->handshake.pc = 0;
    handshake->handshake.coreID = tstate->coreID;
    handshake->handshake.iteration_correction = (ssID == 0);
    handshake->valid = true;

    tstate->lastSignalAddr = ssID_addr;

    handshake_buffer[tid].push(handshake);
    inserted_pool[tid].pop();
    ReleaseLock(&simbuffer_lock);
}

/* ========================================================================== */
VOID ILDJIT_afterSignal(THREADID tid, ADDRINT pc)
{
    GetLock(&simbuffer_lock, tid+1);
//    ignore[tid] = false;
//    cerr << tid <<": After Signal " << hex << pc << dec << endl;

    /* HACKEDY HACKEDY HACK */
    /* We are not simulating and the core still hasn't consumed the wait.
     * Find the dummy handshake in the simulation queue and remove it. */
    if (ExecMode != EXECUTION_MODE_SIMULATE || ignore_all)
    {
        if (!handshake_buffer[tid].empty())
        {
            handshake_container_t* hshake = handshake_buffer[tid].front();
            ASSERTX(hshake->handshake.real == false);
            handshake_buffer[tid].pop();
            inserted_pool[tid].push(hshake);
        }
        ReleaseLock(&simbuffer_lock);
        return;
    }

    thread_state_t* tstate = get_tls(tid);
    unmatchedWaits[tid]--;
    ASSERTX(unmatchedWaits[tid] >= 0);

    while(inserted_pool[tid].empty())
    {
        ReleaseLock(&simbuffer_lock);
        PIN_Yield();
        GetLock(&simbuffer_lock, tid+1);
    }
    handshake_container_t* handshake = inserted_pool[tid].front();

    handshake->isFirstInsn = false;
    handshake->handshake.sleep_thread = false;
    handshake->handshake.resume_thread = true;
    handshake->handshake.real = false;
    handshake->handshake.pc = 0;
    handshake->handshake.coreID = tstate->coreID;
    handshake->handshake.in_critical_section = (num_threads > 1) && (unmatchedWaits[tid] > 0);
    handshake->handshake.iteration_correction = false;
    handshake->valid = true;

    handshake_buffer[tid].push(handshake);
    inserted_pool[tid].pop();

    /* Don't insert signals in single-core mode */
    if (num_threads < 2) {
        ReleaseLock(&simbuffer_lock);
        return;
    }

    /* Insert signal instruction in pipeline */
    while(inserted_pool[tid].empty())
    {
        ReleaseLock(&simbuffer_lock);
        PIN_Yield();
        GetLock(&simbuffer_lock, tid+1);
    }

    handshake = inserted_pool[tid].front();

    handshake->isFirstInsn = false;
    handshake->handshake.sleep_thread = false;
    handshake->handshake.resume_thread = false;
    handshake->handshake.real = false;
    handshake->handshake.coreID = tstate->coreID;
    handshake->handshake.iteration_correction = false;
    handshake->valid = true;

    handshake->handshake.pc = pc;
    handshake->handshake.npc = pc + sizeof(st_template);
    handshake->handshake.tpc = pc + sizeof(st_template);
    handshake->handshake.brtaken = false;
    memcpy(handshake->handshake.ins, st_template, sizeof(st_template));
    // Address comes right after opcode and MoodRM bytes
    *(INT32*)(&handshake->handshake.ins[2]) = tstate->lastSignalAddr;

//    cerr << tid << ": Vodoo store instruction " << hex << pc << " ID: " << tstate->lastSignalAddr << dec << endl;
    handshake_buffer[tid].push(handshake);
    inserted_pool[tid].pop();

    ReleaseLock(&simbuffer_lock);
}


/* ========================================================================== */
VOID ILDJIT_setAffinity(THREADID tid, INT32 coreID)
{
    cerr << tid << ": assigned to core " << coreID << endl;
    ASSERTX(coreID >= 0 && coreID < num_threads);
    thread_state_t* tstate = get_tls(tid);
    tstate->coreID = coreID;
    core_threads[coreID] = tid;
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
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_startParallelLoop),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_endParallelLoop");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_endParallelLoop ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_endParallelLoop),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
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

    rtn = RTN_FindByName(img, "MOLECOOL_beforeWait");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_beforeWait ";
#endif
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
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_afterWait ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(ILDJIT_afterWait),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_CALL_ORDER, CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_beforeSignal");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_beforeSignal ";
#endif
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
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_afterSignal ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(ILDJIT_afterSignal),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_CALL_ORDER, CALL_ORDER_LAST,
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
