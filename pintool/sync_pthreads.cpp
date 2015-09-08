/*
 * ptheads-specific functions to handle synchronization.
 * Copyright, Svilen Kanev, 2012
 */
#include <map>
#include <queue>

#include "boost_interprocess.h"

#include "feeder.h"
#include "multiprocess_shared.h"
#include "BufferManagerProducer.h"

#include "scheduler.h"
#include "sync_pthreads.h"

KNOB<BOOL> KnobPthreads(
    KNOB_MODE_WRITEONCE, "pintool", "pthreads", "false", "Special-case pthreads synchronization");

VOID PTHREAD_beforeJoin(THREADID tid) {
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;

    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid + 1);
    tstate->ignore = true;
    lk_unlock(&tstate->lock);

    handshake_container_t* handshake = xiosim::buffer_management::GetBuffer(tstate->tid);
    handshake->flags.valid = true;
    handshake->flags.real = false;
    handshake->flags.giveCoreUp = true;
    handshake->flags.giveUpReschedule = true;
    xiosim::buffer_management::ProducerDone(tstate->tid);

    xiosim::buffer_management::FlushBuffers(tstate->tid);
}

VOID PTHREAD_beforeMutexLock(THREADID tid) {
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;

    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid + 1);
    tstate->ignore = true;
    lk_unlock(&tstate->lock);

    handshake_container_t* handshake = xiosim::buffer_management::GetBuffer(tstate->tid);
    handshake->flags.valid = true;
    handshake->flags.real = false;
    handshake->flags.giveCoreUp = true;
    handshake->flags.giveUpReschedule = true;
    xiosim::buffer_management::ProducerDone(tstate->tid);

    xiosim::buffer_management::FlushBuffers(tstate->tid);
}

VOID PTHREAD_beforeCondWait(THREADID tid) {
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;

    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid + 1);
    tstate->ignore = true;
    lk_unlock(&tstate->lock);

    handshake_container_t* handshake = xiosim::buffer_management::GetBuffer(tstate->tid);
    handshake->flags.valid = true;
    handshake->flags.real = false;
    handshake->flags.giveCoreUp = true;
    handshake->flags.giveUpReschedule = true;
    xiosim::buffer_management::ProducerDone(tstate->tid);

    xiosim::buffer_management::FlushBuffers(tstate->tid);
}

VOID PTHREAD_stopIgnore(THREADID tid) {
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;

    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid + 1);
    tstate->ignore = false;
    lk_unlock(&tstate->lock);
}

VOID AddPthreadsCallbacks(IMG img) {
    RTN rtn;
    rtn = RTN_FindByName(img, "pthread_join");
    if (RTN_Valid(rtn)) {
        cerr << IMG_Name(img) << ": phtread_join" << endl;

        RTN_Open(rtn);
        RTN_InsertCall(rtn,
                       IPOINT_BEFORE,
                       AFUNPTR(PTHREAD_beforeJoin),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER,
                       CALL_ORDER_FIRST,
                       IARG_END);
        RTN_InsertCall(rtn,
                       IPOINT_AFTER,
                       AFUNPTR(PTHREAD_stopIgnore),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER,
                       CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "pthread_mutex_lock");
    if (RTN_Valid(rtn)) {
        cerr << IMG_Name(img) << ": phtread_mutex_lock" << endl;

        RTN_Open(rtn);
        RTN_InsertCall(rtn,
                       IPOINT_BEFORE,
                       AFUNPTR(PTHREAD_beforeMutexLock),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER,
                       CALL_ORDER_FIRST,
                       IARG_END);
        RTN_InsertCall(rtn,
                       IPOINT_AFTER,
                       AFUNPTR(PTHREAD_stopIgnore),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER,
                       CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "pthread_cond_wait");
    if (RTN_Valid(rtn)) {
        cerr << IMG_Name(img) << ": phtread_cond_wait" << endl;

        RTN_Open(rtn);
        RTN_InsertCall(rtn,
                       IPOINT_BEFORE,
                       AFUNPTR(PTHREAD_beforeCondWait),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER,
                       CALL_ORDER_FIRST,
                       IARG_END);
        RTN_InsertCall(rtn,
                       IPOINT_AFTER,
                       AFUNPTR(PTHREAD_stopIgnore),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER,
                       CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }
}
