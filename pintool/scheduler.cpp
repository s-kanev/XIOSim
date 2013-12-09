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

#include "pin.H"
#include "instlib.H"
using namespace INSTLIB;

#include "shared_map.h"
#include "shared_unordered_map.h"

#include "../interface.h"
#include "multiprocess_shared.h"

#include "feeder.h"
#include "scheduler.h"

#include "../zesto-core.h"
#include "../zesto-structs.h"

struct RunQueue {
    RunQueue() {
        lk_init(&lk);
        last_reschedule = 0;
    }

    tick_t last_reschedule;

    XIOSIM_LOCK lk;
    // XXX: SHARED -- lock protects those
    queue<pid_t> q;
    // XXX: END SHARED
};

static RunQueue * run_queues;

static INT32 last_coreID;

/* Update shm array of running threads */
static void UpdateSHMRunqueues(int coreID, pid_t tid)
{
    lk_lock(lk_coreThreads, 1);
    coreThreads[coreID] = tid;
    if (tid != INVALID_THREADID)
        threadCores->operator[](tid) = coreID;
    lk_unlock(lk_coreThreads);
}

static void UpdateSHMThreadCore(pid_t tid, int coreID)
{
    if (tid == INVALID_THREADID)
        return;
    lk_lock(lk_coreThreads, 1);
    threadCores->operator[](tid) = coreID;
    lk_unlock(lk_coreThreads);
}

static void RemoveSHMThread(pid_t tid)
{
    lk_lock(lk_coreThreads, 1);
    threadCores->erase(tid);
    lk_unlock(lk_coreThreads);
}

/* ========================================================================== */
VOID InitScheduler(INT32 num_cores)
{
    run_queues = new RunQueue[num_cores];
    last_coreID = 0;
}

/* ========================================================================== */
VOID ScheduleNewThread(pid_t tid)
{
    if (GetSHMThreadCore(tid) != (UINT32)-1) {
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
    else {
        UpdateSHMThreadCore(tid, -1);
    }

    run_queues[last_coreID].q.push(tid);
    lk_unlock(&run_queues[last_coreID].lk);

    last_coreID  = (last_coreID + 1) % KnobNumCores.Value();
}

/* ========================================================================== */
VOID DescheduleActiveThread(INT32 coreID)
{
    lk_lock(&run_queues[coreID].lk, 1);

    pid_t tid = run_queues[coreID].q.front();
    run_queues[coreID].last_reschedule = cores[coreID]->sim_cycle;

    lk_lock(printing_lock, 1);
    cerr << "Descheduling thread " << tid << " at coreID  " << coreID << endl;
    lk_unlock(printing_lock);

    /* Deallocate thread state -- XXX: send IPC back to feeder */
/*    thread_state_t* tstate = get_tls(tid);
    delete tstate;
    PIN_DeleteThreadDataKey(tid);
    lk_lock(&thread_list_lock, 1);
    thread_list.remove(tid);
    lk_unlock(&thread_list_lock);
*/
    RemoveSHMThread(tid);
    run_queues[coreID].q.pop();

    pid_t new_tid = INVALID_THREADID;
    if (!run_queues[coreID].q.empty()) {
        new_tid = run_queues[coreID].q.front();

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
    pid_t tid = run_queues[coreID].q.front();
    run_queues[coreID].last_reschedule = cores[coreID]->sim_cycle;

    lk_lock(printing_lock, tid+1);
    cerr << "Thread " << tid << " giving up on core " << coreID << endl;
    lk_unlock(printing_lock);

    run_queues[coreID].q.pop();

    pid_t new_tid = INVALID_THREADID;
    if (!run_queues[coreID].q.empty()) {
        new_tid = run_queues[coreID].q.front();

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

        lk_lock(printing_lock, tid+1);
        cerr << "Rescheduling " << tid << " on core " << coreID << endl;
        lk_unlock(printing_lock);
    }
    lk_unlock(&run_queues[coreID].lk);

    UpdateSHMRunqueues(coreID, new_tid);

    if (new_tid != tid)
        UpdateSHMThreadCore(tid, -1);
}

/* ========================================================================== */
pid_t GetCoreThread(INT32 coreID)
{
    pid_t result;
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
