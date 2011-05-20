#ifndef __MOLECOOL_PIN__
#define __MOLECOOL_PIN__

/* 
 * ILDJIT-specific functions for zesto feeder 
 * Copyright, Svilen Kanev, 2011
 */

// True if ILDJIT has finished compilation and is executing user code
BOOL ILDJIT_executionStarted = false;

// True if a non-compiler thread is being created
BOOL ILDJIT_executorCreation = false;

PIN_LOCK ildjit_lock;

/* ========================================================================== */
VOID MOLECOOL_Init()
{
    InitLock(&ildjit_lock);
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
VOID ILDJIT_ThreadStarting(THREADID tid)
{
    GetLock(&ildjit_lock, 1);

    /* We are stopping thread creation here, beacuse we can capture the real
     * thread creation in Pin only on starting the thread (first insn), which
     * happens after the actual syscalls.
     * XXX: This way we can capture the creation of some compiler threads, 
     * but this is generally fine, since they won't get executed */
    ILDJIT_executorCreation = false;

    ILDJIT_executionStarted = true;
    CODECACHE_FlushCache();

#ifdef ZESTO_PIN_DBG
    cerr << "Starting execution, TID: " << tid << endl;
#endif

    ReleaseLock(&ildjit_lock);
}

/* ========================================================================== */
VOID ILDJIT_ThreadStopping(THREADID tid)
{
    GetLock(&ildjit_lock, 1);

    ILDJIT_executionStarted = false;
    CODECACHE_FlushCache();

#ifdef ZESTO_PIN_DBG
    cerr << "Stopping execution, TID: " << tid << endl;
#endif

    ReleaseLock(&ildjit_lock);
}

/* ========================================================================== */
VOID ILDJIT_ExecutorCreate(THREADID tid)
{
    GetLock(&ildjit_lock, 1);

    ILDJIT_executorCreation = true;

#ifdef ZESTO_PIN_DBG
    cerr << "Starting creation, TID: " << tid << endl;
#endif

    ReleaseLock(&ildjit_lock);
}

/* ========================================================================== */
VOID ILDJIT_ExecutorCreateEnd(THREADID tid)
{
    //Dummy, actual work now done in ILDJIT_ThreadStarting
}

/* ========================================================================== */
VOID AddILDJITCallbacks(IMG img)
{
#ifdef ZESTO_PIN_DBG
    cerr << "Adding ILDJIT callbacks: "; 
#endif

    //Interface to ildjit
    RTN rtn = RTN_FindByName(img, "MOLECOOL_executionStarted");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_codeExecutionStarted ";
#endif
        RTN_Open(rtn);
        
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_ThreadStarting),
                       IARG_THREAD_ID,
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
                       IARG_END);

        RTN_Close(rtn);
    }

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

#ifdef ZESTO_PIN_DBG
    cerr << endl;
#endif
}
    

#endif /* __MOLECOOL_PIN__ */
