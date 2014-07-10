#include <map>
#include <queue>

#include "feeder.h"
#include "../buffer.h"
#include "BufferManager.h"
#include "machsuite.h"

//XXX: ROIs really really need to be abstracted cleanly.

KNOB<BOOL> KnobMachsuite(KNOB_MODE_WRITEONCE,      "pintool",
        "machsuite", "false", "Add machsuite hooks");

VOID Machsuite_BeginROI(THREADID tid, ADDRINT pc)
{
    PPointHandler(CONTROL_START, NULL, NULL, (VOID*)pc, tid);
}

VOID Machsuite_EndROI(THREADID tid, ADDRINT pc)
{
    /* Ignore subsequent instructions that we may see on this thread */
    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = true;
    lk_unlock(&tstate->lock);

    /* Mark this thread for descheduling */
    handshake_container_t *handshake = handshake_buffer.get_buffer(tid);
    handshake->flags.giveCoreUp = true;
    handshake->flags.giveUpReschedule = false;
    handshake->flags.valid = true;
    handshake->handshake.real = false;
    handshake_buffer.producer_done(tid, true);

    handshake_buffer.flushBuffers(tid);

    /* Let core pipes drain an pause simulation */
    PPointHandler(CONTROL_STOP, NULL, NULL, (VOID*)pc, tid);
}

VOID AddMachsuiteCallbacks(IMG img)
{
    RTN rtn;
    rtn = RTN_FindByName(img, "run_benchmark");
    if (RTN_Valid(rtn))
    {
        cerr << IMG_Name(img) << ": run_benchmark" << endl;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(Machsuite_BeginROI),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER, CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "run_benchmark");
    if (RTN_Valid(rtn))
    {
        cerr << IMG_Name(img) << ": run_benchmark" << endl;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(Machsuite_EndROI),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER, CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }
}
