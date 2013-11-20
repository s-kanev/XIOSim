/*
 * ptheads-specific functions to handle synchronization.
 * Copyright, Svilen Kanev, 2012
 */

// Sam: These includes are needed to pass compilation. I'm not sure why.
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/unordered_map.hpp>

#include <map>
#include <queue>

#include "feeder.h"
#include "scheduler.h"
#include "sync_pthreads.h"
#include "../buffer.h"
#include "BufferManager.h"

KNOB<BOOL> KnobPthreads(KNOB_MODE_WRITEONCE,      "pintool",
        "pthreads", "false", "Special-case pthreads synchronization");

VOID PTHREAD_beforeJoin(THREADID tid)
{
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;

    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = true;
    lk_unlock(&tstate->lock);

    handshake_container_t *handshake = handshake_buffer.get_buffer(tid);
    handshake->flags.valid = true;
    handshake->handshake.real = false;
    handshake->flags.giveCoreUp = true;
    handshake->flags.giveUpReschedule = true;
    handshake_buffer.producer_done(tid);

    handshake_buffer.flushBuffers(tid);
}

VOID PTHREAD_beforeMutexLock(THREADID tid)
{
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;

    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = true;
    lk_unlock(&tstate->lock);

    handshake_container_t *handshake = handshake_buffer.get_buffer(tid);
    handshake->flags.valid = true;
    handshake->handshake.real = false;
    handshake->flags.giveCoreUp = true;
    handshake->flags.giveUpReschedule = true;
    handshake_buffer.producer_done(tid);

    handshake_buffer.flushBuffers(tid);
}

VOID PTHREAD_beforeCondWait(THREADID tid)
{
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;

    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = true;
    lk_unlock(&tstate->lock);

    handshake_container_t *handshake = handshake_buffer.get_buffer(tid);
    handshake->flags.valid = true;
    handshake->handshake.real = false;
    handshake->flags.giveCoreUp = true;
    handshake->flags.giveUpReschedule = true;
    handshake_buffer.producer_done(tid);

    handshake_buffer.flushBuffers(tid);
}

VOID PTHREAD_stopIgnore(THREADID tid)
{
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;

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
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(PTHREAD_stopIgnore),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER, CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "pthread_mutex_lock");
    if (RTN_Valid(rtn))
    {
        cerr << IMG_Name(img) << ": phtread_mutex_lock" << endl;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(PTHREAD_beforeMutexLock),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER, CALL_ORDER_FIRST,
                       IARG_END);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(PTHREAD_stopIgnore),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER, CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "pthread_cond_wait");
    if (RTN_Valid(rtn))
    {
        cerr << IMG_Name(img) << ": phtread_cond_wait" << endl;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(PTHREAD_beforeCondWait),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER, CALL_ORDER_FIRST,
                       IARG_END);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(PTHREAD_stopIgnore),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER, CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }
}
