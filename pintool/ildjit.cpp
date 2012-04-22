/* 
 * ILDJIT-specific functions for zesto feeder 
 * Copyright, Svilen Kanev, 2012
 */

#include <map>
#include "feeder.h"
#include "ildjit.h"
#include "fluffy.h"

// True if ILDJIT has finished compilation and is executing user code
BOOL ILDJIT_executionStarted = false;

// True if a non-compiler thread is being created
BOOL ILDJIT_executorCreation = false;

PIN_LOCK ildjit_lock;

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
}

/* ========================================================================== */
VOID ILDJIT_endParallelLoop(THREADID tid, ADDRINT loop)
{
}

/* ========================================================================== */
VOID ILDJIT_startIteration(THREADID tid)
{
}

/* ========================================================================== */
VOID ILDJIT_beforeWait(THREADID tid, ADDRINT ssID)
{
}

/* ========================================================================== */
VOID ILDJIT_afterWait(THREADID tid, ADDRINT ssID)
{
}

/* ========================================================================== */
VOID ILDJIT_beforeSignal(THREADID tid, ADDRINT ssID)
{
}

/* ========================================================================== */
VOID ILDJIT_afterSignal(THREADID tid, ADDRINT ssID)
{
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
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_afterWait),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
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
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_afterSignal),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
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
