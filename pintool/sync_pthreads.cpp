/*
 * ptheads-specific functions to handle synchronization.
 * Copyright, Svilen Kanev, 2012
 */

#include <map>
#include <queue>

#include "feeder.h"
#include "scheduler.h"
#include "sync_pthreads.h"
#include "Buffer.h"
#include "BufferManager.h"

KNOB<BOOL> KnobPthreads(KNOB_MODE_WRITEONCE,      "pintool",
        "pthreads", "false", "Special-case pthreads synchronization");

VOID PTHREAD_beforeJoin(THREADID tid)
{
    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = true;
    lk_unlock(&tstate->lock);

    handshake_container_t *handshake = handshake_buffer.get_buffer(tid);
    handshake->flags.valid = true;
    handshake->handshake.real = false;
    handshake->flags.giveCoreUp = true;
    handshake_buffer.producer_done(tid);
}

VOID PTHREAD_afterJoin(THREADID tid)
{
    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = false;
    lk_unlock(&tstate->lock);
}

VOID AddPthreadsCallbacks(IMG img)
{
    RTN rtn;
    rtn = RTN_FindByName(img, "pthread_join");
    if (RTN_Valid(rtn))
    {
        cerr << IMG_Name(img) << ": phtread_join" << endl;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(PTHREAD_beforeJoin),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER, CALL_ORDER_FIRST,
                       IARG_END);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(PTHREAD_afterJoin),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER, CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }
}
