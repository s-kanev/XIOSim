/*
 * ptheads-specific functions to handle synchronization.
 * Copyright, Svilen Kanev, 2012
 */
#include <map>
#include <queue>

#include "feeder.h"

#include "scheduler.h"
#include "sync_pthreads.h"

KNOB<BOOL> KnobPthreads(
    KNOB_MODE_WRITEONCE, "pintool", "pthreads", "false", "Special-case pthreads synchronization");

static void PTHREAD_stopIgnore(THREADID tid);

/* pthread_join takes a pthread_t, which should be opaque.
 * Not so much if we're a simulator pretending to be the kernel.
 * In glibc, join just waits on a futex in the TCB, which
 * the kernel signals when a thread exits.
 * Before we implement proper futex waiting, let's just grab
 * the thread's kernel pid_t and have our scheduler wait for it.
 * This obviously won't work for crazy implementations with m:n
 * threading, but glibc on linux is just 1:1. */

static pid_t pid_from_pthread_t(pthread_t pthread) {
    /* pthread_t is just a pointer to struct pthread (defined in nptl/descr.h).
     * There are 26 void* before the tid field (24 padding, 2 in list_t),
     * hence that part of the offset.
     * No idea where the 0x200 comes from, but it's there. */
    const uintptr_t offset = 0x200 + 26 * sizeof(void*);
    uintptr_t ptr = static_cast<uintptr_t>(pthread) + offset;
    return *(pid_t*)(ptr);
}

VOID PTHREAD_beforeJoin(THREADID tid, ADDRINT arg) {
    pid_t internal_pid = pid_from_pthread_t(static_cast<pthread_t>(arg));

#ifdef PTHREADS_DEBUG
    thread_state_t* tstate = get_tls(tid);
    cerr << "[" << tstate->tid << "]" << " pthread_join(" << internal_pid << ")" << endl;
    cerr.flush();
#endif
    /* Make sure we parsed the pid correctly, i.e. we're trying to join a thread
     * which we've already seen. This could fail on a different platform / if the
     * field order in struct pthread changes. */
    ASSERTX(global_to_local_tid.count(internal_pid) > 0);

    AddBlockedHandshake(tid, internal_pid);
}

VOID PTHREAD_afterJoin(THREADID tid) {
#ifdef PTHREADS_DEBUG
    thread_state_t* tstate = get_tls(tid);
    cerr << "[" << tstate->tid << "]" << " After pthread_join(XXX)" << endl;
#endif

    PTHREAD_stopIgnore(tid);
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
        cerr << IMG_Name(img) << ": pthread_join" << endl;

        RTN_Open(rtn);
        RTN_InsertCall(rtn,
                       IPOINT_BEFORE,
                       AFUNPTR(PTHREAD_beforeJoin),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE,
                       0,
                       IARG_CALL_ORDER,
                       CALL_ORDER_FIRST,
                       IARG_END);
        RTN_InsertCall(rtn,
                       IPOINT_AFTER,
                       AFUNPTR(PTHREAD_afterJoin),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER,
                       CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }
}
