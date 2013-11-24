/*
 * Scheduling functions for mapping app threads to cores
 * Copyright, Svilen Kanev, 2013
 */

// Sam: These includes are needed to pass compilation. I'm not sure why.
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/unordered_map.hpp>

#include <queue>
#include <map>

#include "feeder.h"
#include "../buffer.h"
#include "BufferManager.h"
#include "scheduler.h"

#include "../zesto-core.h"
#include "../zesto-structs.h"
#include "multiprocess_shared.h"

struct RunQueue {
    RunQueue() {
        lk_init(&lk);
        last_reschedule = 0;
    }

    tick_t last_reschedule;

    XIOSIM_LOCK lk;
    // XXX: SHARED -- lock protects those
    queue<THREADID> q;
    // XXX: END SHARED
};

static RunQueue * run_queues;

static INT32 last_coreID;

/* Update shm array of running threads */
static void UpdateSHMRunqueues(int coreID, THREADID tid)
{
    lk_lock(lk_coreThreads, 1);
    coreThreads[coreID] = tid;
    lk_unlock(lk_coreThreads);
}

/* ========================================================================== */
VOID InitScheduler(INT32 num_cores)
{
    run_queues = new RunQueue[num_cores];
    last_coreID = 0;
}

/* ========================================================================== */
VOID ScheduleNewThread(THREADID tid)
{
    thread_state_t* tstate = get_tls(tid);
    if (tstate->coreID != (UINT32)-1) {
        lk_lock(printing_lock, 1);
        cerr << "ScheduleNewThread: thread " << tid << " already scheduled, ignoring." << endl;
        lk_unlock(printing_lock);
        return;
    }

    lk_lock(&run_queues[last_coreID].lk, 1);
    if (run_queues[last_coreID].q.empty() && !is_core_active(last_coreID)) {
        activate_core(last_coreID);
        UpdateSHMRunqueues(last_coreID, tid);
    }

    run_queues[last_coreID].q.push(tid);
    lk_unlock(&run_queues[last_coreID].lk);

    tstate->coreID = last_coreID;

    
    last_coreID  = (last_coreID + 1) % KnobNumCores.Value();
}

/* ========================================================================== */
VOID HardcodeSchedule(THREADID tid, INT32 coreID)
{
    lk_lock(&run_queues[coreID].lk, 1);
    ASSERTX(run_queues[coreID].q.empty());
    activate_core(coreID);
    run_queues[coreID].q.push(tid);
    lk_unlock(&run_queues[coreID].lk);

    UpdateSHMRunqueues(coreID, tid);

    thread_state_t* tstate = get_tls(tid);
    tstate->coreID = coreID;
}

/* ========================================================================== */
VOID DescheduleActiveThread(INT32 coreID)
{
    lk_lock(&run_queues[coreID].lk, 1);

    THREADID tid = run_queues[coreID].q.front();
    run_queues[coreID].last_reschedule = cores[coreID]->sim_cycle;

    lk_lock(printing_lock, 1);
    cerr << "Descheduling thread " << tid << " at coreID  " << coreID << endl;
    lk_unlock(printing_lock);

    /* Deallocate thread state */
    thread_state_t* tstate = get_tls(tid);
    delete tstate;
    PIN_DeleteThreadDataKey(tid);
    lk_lock(&thread_list_lock, 1);
    thread_list.remove(tid);
    lk_unlock(&thread_list_lock);

    run_queues[coreID].q.pop();

    THREADID new_tid = INVALID_THREADID;
    if (!run_queues[coreID].q.empty()) {
        new_tid = run_queues[coreID].q.front();
        thread_state_t *new_tstate = get_tls(new_tid);
        new_tstate->coreID = coreID;

        lk_lock(printing_lock, tid+1);
        cerr << "Thread " << new_tid << " going on core " << coreID << endl;
        lk_unlock(printing_lock);
    } else {
        /* No more work to do, let other cores progress */
        deactivate_core(coreID);
    }

    lk_unlock(&run_queues[coreID].lk);
    UpdateSHMRunqueues(coreID, new_tid);
}

/* ========================================================================== */
/* XXX: This is called from a sim thread -- the one that frees up the core */
VOID GiveUpCore(INT32 coreID, BOOL reschedule_thread)
{
    lk_lock(&run_queues[coreID].lk, 1);
    THREADID tid = run_queues[coreID].q.front();
    run_queues[coreID].last_reschedule = cores[coreID]->sim_cycle;

    lk_lock(printing_lock, tid+1);
    cerr << "Thread " << tid << " giving up on core " << coreID << endl;
    lk_unlock(printing_lock);

    thread_state_t *tstate = get_tls(tid);
    tstate->coreID = -1;

    run_queues[coreID].q.pop();

    THREADID new_tid = INVALID_THREADID;
    if (!run_queues[coreID].q.empty()) {
        new_tid = run_queues[coreID].q.front();
        thread_state_t *new_tstate = get_tls(new_tid);
        new_tstate->coreID = coreID;

        lk_lock(printing_lock, tid+1);
        cerr << "Thread " << new_tid << " going on core " << coreID << endl;
        lk_unlock(printing_lock);
    }
    else if (!reschedule_thread) {
        /* No more work to do, let core sleep */
        deactivate_core(coreID);
    }

    if (reschedule_thread) {
        new_tid = tid;
        run_queues[coreID].q.push(tid);

        thread_state_t* tstate = get_tls(tid);
        tstate->coreID = coreID;

        lk_lock(printing_lock, tid+1);
        cerr << "Rescheduling " << tid << " on core " << coreID << endl;
        lk_unlock(printing_lock);
    }
    lk_unlock(&run_queues[coreID].lk);

    UpdateSHMRunqueues(coreID, new_tid);
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

/* ========================================================================== */
BOOL NeedsReschedule(INT32 coreID)
{
    tick_t since_schedule = cores[coreID]->sim_cycle - run_queues[coreID].last_reschedule;
    return (knobs.scheduler_tick > 0) && (since_schedule > knobs.scheduler_tick);
}
