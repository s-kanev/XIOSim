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

/* ========================================================================== */
VOID MOLECOOL_Init()
{
    InitLock(&ildjit_lock);

    FLUFFY_Init();
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
    /* This is called from the middle of a wait spin loop.
     * For this thread only, ignore the end of the wait, so we
     * can actually start simulating and unblock everyone else.
     */
    thread_state_t* tstate = get_tls(tid);
    tstate->firstWait = true;

    if (KnobFluffy.Value().empty())
        PPointHandler(CONTROL_START, NULL, NULL, (VOID*)ip, tid);

    ReleaseLock(&ildjit_lock);
}

/* ========================================================================== */
VOID ILDJIT_endSimulation(THREADID tid, ADDRINT ip)
{
    GetLock(&ildjit_lock, 1);

    //ILDJIT_executionStarted = false;

//#ifdef ZESTO_PIN_DBG
    cerr << "Stopping execution, TID: " << tid << endl;
//#endif

    if (KnobFluffy.Value().empty())
        StopSimulation(tid);

    ReleaseLock(&ildjit_lock);
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

/* ========================================================================== */
VOID ILDJIT_startParallelLoop(THREADID tid, ADDRINT loop)
{
    // For now, do nothing
}

/* ========================================================================== */
VOID ILDJIT_endParallelLoop(THREADID tid, ADDRINT loop)
{
    // For now, do nothing
}

/* ========================================================================== */
VOID ILDJIT_startIteration(THREADID tid)
{
    // For now, do nothing
}

/* ========================================================================== */
VOID ILDJIT_beforeWait(THREADID tid, ADDRINT ssID_addr, ADDRINT ssID, ADDRINT pc)
{
    GetLock(&simbuffer_lock, tid+1);
//    ignore[tid] = true;
    cerr << tid <<": Before Wait " << hex << ssID << dec << endl;

    ASSERTX(!inserted_pool[tid].empty());
    handshake_container_t* handshake = inserted_pool[tid].front();

    handshake->isFirstInsn = false;
    handshake->handshake.sleep_thread = true;
    handshake->handshake.resume_thread = false;
    handshake->handshake.real = false;
    thread_state_t* tstate = get_tls(tid);
    handshake->handshake.coreID = tstate->coreID;
    handshake->handshake.iteration_correction = (ssID == 0);
    handshake->valid = true;

    tstate->lastSignalID = ssID_addr;

    handshake_buffer[tid].push(handshake);
    inserted_pool[tid].pop();
    ReleaseLock(&simbuffer_lock);
}

/* ========================================================================== */
VOID ILDJIT_afterWait(THREADID tid, ADDRINT pc)
{
    GetLock(&simbuffer_lock, tid+1);
//    ignore[tid] = false;
//    cerr << tid <<": After Wait "<< hex << pc << dec  << endl;

    /* HACKEDY HACKEDY HACK */
    /* We are not simulating and the core still hasn't consumed the wait.
     * Find the dummy handshake in the simulation queue and remove it. */
    if (ExecMode != EXECUTION_MODE_SIMULATE
        && inserted_pool[tid].empty())
    {
        ASSERTX(!handshake_buffer[tid].empty());
        handshake_container_t* hshake = handshake_buffer[tid].front();
        ASSERTX(hshake->handshake.real == false);
        ASSERTX(hshake->handshake.sleep_thread == true);
        handshake_buffer[tid].pop();
        inserted_pool[tid].push(hshake);
        ReleaseLock(&simbuffer_lock);
        return;
    }

    ASSERTX(!inserted_pool[tid].empty());
    handshake_container_t* handshake = inserted_pool[tid].front();

    thread_state_t* tstate = get_tls(tid);
    handshake->isFirstInsn = false;
    handshake->handshake.sleep_thread = false;
    handshake->handshake.resume_thread = true;
    handshake->handshake.real = false;
    handshake->handshake.coreID = tstate->coreID;
    handshake->handshake.in_critical_section = (tstate->unmatchedWaits > 0);
    handshake->handshake.iteration_correction = false;
    handshake->valid = true;

    tstate->unmatchedWaits++;

    handshake_buffer[tid].push(handshake);
    inserted_pool[tid].pop();

    /* Ignore injecting first wait so we can start simulating */
    if (tstate->firstWait)
    {
        tstate->firstWait = false;
        ReleaseLock(&simbuffer_lock);
        return;
    }

    /* Insert wait instruction in pipeline */
    ASSERTX(!inserted_pool[tid].empty());
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
    *(INT32*)(&handshake->handshake.ins[1]) = tstate->lastSignalID;

    cerr << tid << ": Vodoo load instruction " << hex << pc <<  " ID: " << tstate->lastSignalID << dec << endl;
    handshake_buffer[tid].push(handshake);
    inserted_pool[tid].pop();

    ReleaseLock(&simbuffer_lock);
}

/* ========================================================================== */
VOID ILDJIT_beforeSignal(THREADID tid, ADDRINT ssID, ADDRINT pc)
{
    GetLock(&simbuffer_lock, tid+1);
//    ignore[tid] = true;
    cerr << tid <<": Before Signal " << hex << ssID << dec << endl;

    ASSERTX(!inserted_pool[tid].empty());
    handshake_container_t* handshake = inserted_pool[tid].front();

    handshake->isFirstInsn = false;
    handshake->handshake.sleep_thread = true;
    handshake->handshake.resume_thread = false;
    handshake->handshake.real = false;
    thread_state_t* tstate = get_tls(tid);
    handshake->handshake.coreID = tstate->coreID;
    handshake->handshake.iteration_correction = false;
    handshake->valid = true;

    tstate->lastSignalID = ssID;

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
    if (ExecMode != EXECUTION_MODE_SIMULATE
        && inserted_pool[tid].empty())
    {
        ASSERTX(!handshake_buffer[tid].empty());
        handshake_container_t* hshake = handshake_buffer[tid].front();
        ASSERTX(hshake->handshake.real == false);
        handshake_buffer[tid].pop();
        inserted_pool[tid].push(hshake);
        ReleaseLock(&simbuffer_lock);
        return;
    }

    thread_state_t* tstate = get_tls(tid);
    tstate->unmatchedWaits--;
    ASSERTX(tstate->unmatchedWaits >= 0);

    ASSERTX(!inserted_pool[tid].empty());
    handshake_container_t* handshake = inserted_pool[tid].front();

    handshake->isFirstInsn = false;
    handshake->handshake.sleep_thread = false;
    handshake->handshake.resume_thread = true;
    handshake->handshake.real = false;
    handshake->handshake.coreID = tstate->coreID;
    handshake->handshake.in_critical_section = (tstate->unmatchedWaits > 0);
    handshake->handshake.iteration_correction = false;
    handshake->valid = true;

    handshake_buffer[tid].push(handshake);
    inserted_pool[tid].pop();

    /* Insert signal instruction in pipeline */
    ASSERTX(!inserted_pool[tid].empty());
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
    *(INT32*)(&handshake->handshake.ins[2]) = tstate->lastSignalID;

    cerr << tid << ": Vodoo store instruction " << hex << pc << " ID: " << tstate->lastSignalID << dec << endl;
    handshake_buffer[tid].push(handshake);
    inserted_pool[tid].pop();

    ReleaseLock(&simbuffer_lock);
}


/* ========================================================================== */
VOID AddILDJITCallbacks(IMG img)
{
#ifdef ZESTO_PIN_DBG
    cerr << "Adding ILDJIT callbacks: "; 
#endif

    //Interface to ildjit
    RTN rtn;
/*    rtn = RTN_FindByName(img, "MOLECOOL_executionStarted");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_codeExecutionStarted ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_ThreadStarting),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_executionStop");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_codeExecutionStop ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_ThreadStopping),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_END);
        RTN_Close(rtn);
    }
*/
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

/*    rtn = RTN_FindByName(img, "MOLECOOL_startIteration");
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
*/
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
