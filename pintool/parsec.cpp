#include <map>
#include <queue>

#include "boost_interprocess.h"

#include "feeder.h"
#include "multiprocess_shared.h"
#include "../buffer.h"
#include "BufferManagerProducer.h"

#include "parsec.h"

KNOB<BOOL> KnobParsec(KNOB_MODE_WRITEONCE,      "pintool",
        "parsec", "false", "Add parsec hooks");

VOID Parsec_BeginROI(THREADID tid, ADDRINT pc)
{
    PPointHandler(CONTROL_START, NULL, NULL, (VOID*)pc, tid);
}

VOID Parsec_EndROI(THREADID tid, ADDRINT pc)
{
    /* Ignore subsequent instructions that we may see on this thread */
    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = true;
    lk_unlock(&tstate->lock);

    /* Mark this thread for descheduling */
    handshake_container_t *handshake = xiosim::buffer_management::get_buffer(tstate->tid);
    handshake->flags.giveCoreUp = true;
    handshake->flags.giveUpReschedule = false;
    handshake->flags.valid = true;
    handshake->handshake.real = false;
    xiosim::buffer_management::producer_done(tstate->tid, true);

    xiosim::buffer_management::flushBuffers(tstate->tid);

    /* Let core pipes drain an pause simulation */
    PPointHandler(CONTROL_STOP, NULL, NULL, (VOID*)pc, tid);
}

VOID AddParsecCallbacks(IMG img)
{
    RTN rtn;
    rtn = RTN_FindByName(img, "__parsec_roi_begin");
    if (RTN_Valid(rtn))
    {
        cerr << IMG_Name(img) << ": __parsec_roi_begin" << endl;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(Parsec_BeginROI),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER, CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "__parsec_roi_end");
    if (RTN_Valid(rtn))
    {
        cerr << IMG_Name(img) << ": __parsec_roi_end" << endl;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(Parsec_EndROI),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER, CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
    }
}
