/*
 * Scheduling functions for mapping app threads to cores
 * Copyright, Svilen Kanev, 2013
 */

#include <queue>
#include <map>

#include "feeder.h"
#include "scheduler.h"

struct RunQueue {
    RunQueue() {
        lk_init(&lk);
    }

    queue<THREADID> q;
    XIOSIM_LOCK lk;
};

static RunQueue * run_queues;

static INT32 last_coreID;

/* ========================================================================== */
VOID InitScheduler(INT32 num_cores)
{
    run_queues = new RunQueue[num_threads];
    last_coreID = 0;
}

/* ========================================================================== */
VOID ScheduleNewThread(THREADID tid)
{
    lk_lock(&run_queues[last_coreID].lk, 1);
    run_queues[last_coreID].q.push(tid);
    lk_unlock(&run_queues[last_coreID].lk);

    thread_state_t* tstate = get_tls(tid);
    tstate->coreID = last_coreID;

    last_coreID  = (last_coreID + 1) % num_threads;
}

/* ========================================================================== */
VOID HardcodeSchedule(THREADID tid, INT32 coreID)
{
    lk_lock(&run_queues[coreID].lk, 1);
    ASSERTX(run_queues[coreID].q.empty());
    run_queues[coreID].q.push(tid);
    lk_unlock(&run_queues[coreID].lk);

    thread_state_t* tstate = get_tls(tid);
    tstate->coreID = coreID;
}

/* ========================================================================== */
VOID DescheduleActiveThread(INT32 coreID)
{
    lk_lock(&run_queues[coreID].lk, 1);

    THREADID tid = run_queues[coreID].q.front();

    lk_lock(&printing_lock, 1);
    cerr << "Descheduling thread " << tid << " at coreID  " << coreID << endl;
    lk_unlock(&printing_lock);

    /* Deallocate thread state */
    thread_state_t* tstate = get_tls(tid);
    delete tstate;
    PIN_DeleteThreadDataKey(tid);
    lk_lock(&thread_list_lock, 1);
    thread_list.remove(tid);
    lk_unlock(&thread_list_lock);

    run_queues[coreID].q.pop();

    if (!run_queues[coreID].q.empty()) {
        THREADID new_tid = run_queues[coreID].q.front();
        thread_state_t *new_tstate = get_tls(new_tid);
        new_tstate->coreID = coreID;

        lk_lock(&printing_lock, tid+1);
        cerr << "Thread " << new_tid << " going on core " << coreID << endl;
        lk_unlock(&printing_lock);
    } else {
        /* No more work to do, let other cores progress */
        deactivate_core(coreID);
    }

    lk_unlock(&run_queues[coreID].lk);
}

/* ========================================================================== */
VOID GiveUpCore(INT32 coreID, BOOL reschedule_thread)
{
    lk_lock(&run_queues[coreID].lk, 1);
    THREADID tid = run_queues[coreID].q.front();

    lk_lock(&printing_lock, tid+1);
    cerr << "Thread " << tid << " giving up on core " << coreID << endl;
    lk_unlock(&printing_lock);

    thread_state_t *tstate = get_tls(tid);
    tstate->coreID = -1;

    run_queues[coreID].q.pop();

    if (!run_queues[coreID].q.empty()) {
        THREADID new_tid = run_queues[coreID].q.front();
        thread_state_t *new_tstate = get_tls(new_tid);
        new_tstate->coreID = coreID;

        lk_lock(&printing_lock, tid+1);
        cerr << "Thread " << new_tid << " going on core " << coreID << endl;
        lk_unlock(&printing_lock);
    }
    else if (!reschedule_thread) {
        /* No more work to do, let core sleep */
        deactivate_core(coreID);
    }

    if (reschedule_thread) {
        run_queues[coreID].q.push(tid);

        lk_lock(&printing_lock, tid+1);
        cerr << "Rescheduling " << tid << " on core " << coreID << endl;
        lk_unlock(&printing_lock);
    }


    lk_unlock(&run_queues[coreID].lk);
}

/* ========================================================================== */
THREADID GetCoreThread(INT32 coreID)
{
    THREADID result;
    lk_lock(&run_queues[coreID].lk, 1);
    if (run_queues[coreID].q.empty())
        result = INVALID_THREADID;
    else
        result = run_queues[coreID].q.front();
    lk_unlock(&run_queues[coreID].lk);
    return result;
}

/* ========================================================================== */
BOOL IsCoreBusy(INT32 coreID)
{
    BOOL result;
    lk_lock(&run_queues[coreID].lk, 1);
    result = !run_queues[coreID].q.empty();
    lk_unlock(&run_queues[coreID].lk);
    return result;
}
