/*
 * Scheduling functions for mapping app threads to cores
 * Copyright, Svilen Kanev, 2013
 */


#include <queue>
#include <map>
#include <list>

#include "boost_interprocess.h"

#include "../interface.h"
#include "multiprocess_shared.h"

#include "scheduler.h"

#include "../zesto-core.h"
#include "../zesto-structs.h"

extern int num_cores;

namespace xiosim {

struct RunQueue {
    RunQueue() {
        lk_init(&lk);
        last_reschedule = 0;
    }

    tick_t last_reschedule;

    XIOSIM_LOCK lk;
    // XXX: SHARED -- lock protects those
    std::queue<pid_t> q;
    // XXX: END SHARED
};

static RunQueue * run_queues;

static int last_coreID;
static XIOSIM_LOCK last_coreID_lk;

static std::map<pid_t, int> affinity;
static XIOSIM_LOCK affinity_lk;
static int GetThreadAffinity(pid_t tid);

static void UpdateSHMCoreThread(int coreID, pid_t tid);
static void UpdateSHMThreadCore(pid_t tid, int coreID);
static void RemoveSHMThread(pid_t tid);

/* ========================================================================== */
void InitScheduler(int num_cores)
{
    run_queues = new RunQueue[num_cores];
    last_coreID = 0;
    lk_init(&last_coreID_lk);

    lk_init(&affinity_lk);
}

/* ========================================================================== */
int ScheduleNewThread(pid_t tid)
{
    /* Make sure thread is queued at one and only one runqueue. */
    if (IsSHMThreadSimulatingMaybe(tid)) {
#ifdef SCHEDULER_DEBUG
        lk_lock(printing_lock, 1);
        std::cerr << "ScheduleNewThread: thread " << tid << " already scheduled, ignoring." << std::endl;
        lk_unlock(printing_lock);
#endif
        return INVALID_CORE;
    }

    /* Check if thread has been pinned to a core. */
    int coreID = GetThreadAffinity(tid);

    /* No affinity, just round-robin on cores. */
    if (coreID == INVALID_CORE) {
        /* Atomically read and adjust the next available core */
        lk_lock(&last_coreID_lk, 1);
        coreID = last_coreID;
        /* For now, just round-robin on available cores. */
        last_coreID  = (last_coreID + 1) % num_cores;
        lk_unlock(&last_coreID_lk);
    }

    lk_lock(&run_queues[coreID].lk, 1);
    if (run_queues[coreID].q.empty() && !is_core_active(coreID)) {
        /* We can on @coreID straight away, activate it and update SHM state. */
        activate_core(coreID);
        UpdateSHMThreadCore(tid, coreID);
        UpdateSHMCoreThread(coreID, tid);
    }
    else {
        /* We need to wait for a reschedule */
        UpdateSHMThreadCore(tid, INVALID_CORE);
    }

#ifdef SCHEDULER_DEBUG
    lk_lock(printing_lock, 1);
    std::cerr << "ScheduleNewThread: thread " << tid << " on core " << coreID << std::endl;
    lk_unlock(printing_lock);
#endif

    run_queues[coreID].q.push(tid);
    lk_unlock(&run_queues[coreID].lk);
    return coreID;
}

/* Threads are ordered by affinity to "virtual cores".
 * We grab the first @core_allocation ones and actually schedule
 * them to real cores (which could be different from the virtual
 * ones, but we preserve the ordering between threads). */
/* ========================================================================== */
void ScheduleProcessThreads(int asid, std::list<pid_t> threads)
{
    CoreSet scheduled_cores;

    /* Hardcoded policy for now, each process gets a
     * hardcoded contiguous subset of cores. */
    int offset = asid * (num_cores / *num_processes);

    int i=0;
    for (pid_t tid : threads) {
        SetThreadAffinity(tid, offset + i);
        int coreID = ScheduleNewThread(tid);
        assert(coreID == offset + i);
        scheduled_cores.insert(coreID);
        i++;
    }

    UpdateProcessCoreSet(asid, scheduled_cores);
}

/* ========================================================================== */
void DescheduleActiveThread(int coreID)
{
    lk_lock(&run_queues[coreID].lk, 1);

    pid_t tid = run_queues[coreID].q.front();
    run_queues[coreID].last_reschedule = cores[coreID]->sim_cycle;

#ifdef SCHEDULER_DEBUG
    lk_lock(printing_lock, 1);
    std::cerr << "Descheduling thread " << tid << " at coreID  " << coreID << std::endl;
    lk_unlock(printing_lock);
#endif

    /* Deallocate thread state -- XXX: send IPC back to feeder */
/*    thread_state_t* tstate = get_tls(tid);
    delete tstate;
    PIN_DeleteThreadDataKey(tid);
    lk_lock(&thread_list_lock, 1);
    thread_list.remove(tid);
    lk_unlock(&thread_list_lock);
*/
    /* This thread is no more. */
    RemoveSHMThread(tid);
    run_queues[coreID].q.pop();

    pid_t new_tid = INVALID_THREADID;
    if (!run_queues[coreID].q.empty()) {
        new_tid = run_queues[coreID].q.front();

        /* Let SHM know @new_tid is running on @coreID */
        UpdateSHMThreadCore(new_tid, coreID);

#ifdef SCHEDULER_DEBUG
        lk_lock(printing_lock, tid+1);
        std::cerr << "Thread " << new_tid << " going on core " << coreID << std::endl;
        lk_unlock(printing_lock);
#endif
    } else {
        /* No more work to do, let other cores progress */
        deactivate_core(coreID);
    }

    /* Let SHM know @coreID has @new_tid (potentially nothing) */
    UpdateSHMCoreThread(coreID, new_tid);
    lk_unlock(&run_queues[coreID].lk);
}

/* ========================================================================== */
/* XXX: This is called from a sim thread -- the one that frees up the core */
void GiveUpCore(int coreID, bool reschedule_thread)
{
    lk_lock(&run_queues[coreID].lk, 1);
    pid_t tid = run_queues[coreID].q.front();
    run_queues[coreID].last_reschedule = cores[coreID]->sim_cycle;

#ifdef SCHEDULER_DEBUG
    lk_lock(printing_lock, tid+1);
    std::cerr << "Thread " << tid << " giving up on core " << coreID << std::endl;
    lk_unlock(printing_lock);
#endif

    /* This thread gets descheduled. */
    run_queues[coreID].q.pop();

    pid_t new_tid;
    if (!run_queues[coreID].q.empty()) {
        /* There is another thread waiting for the core. It gets scheduled. */
        new_tid = run_queues[coreID].q.front();

#ifdef SCHEDULER_DEBUG
        lk_lock(printing_lock, tid+1);
        std::cerr << "Thread " << new_tid << " going on core " << coreID << std::endl;
        lk_unlock(printing_lock);
#endif
    }
    else if (!reschedule_thread) {
        /* No more work to do, let core sleep */
        deactivate_core(coreID);
        new_tid = INVALID_THREADID;
    } else {
        /* Thread is the only one waiting for this core, reschedule is moot. */
        new_tid = tid;
    }
    /* Let SHM know @coreID has @new_tid (potentially nothing) */
    UpdateSHMCoreThread(coreID, new_tid);

    if (reschedule_thread) {
        /* Reschedule at the back of (possibly empty) runqueue for @coreID. */
        run_queues[coreID].q.push(tid);

#ifdef SCHEDULER_DEBUG
        lk_lock(printing_lock, tid+1);
        std::cerr << "Rescheduling " << tid << " on core " << coreID << std::endl;
        lk_unlock(printing_lock);
#endif

        /* If old thread is requeued behind a new thread, update its SHM status. */
        if (new_tid != tid)
            UpdateSHMThreadCore(new_tid, INVALID_CORE);
    }
    else {
        /* For all we know in this case, old thread will never be scheduled again. */
        RemoveSHMThread(tid);
    }

    lk_unlock(&run_queues[coreID].lk);
}

/* ========================================================================== */
pid_t GetCoreThread(int coreID)
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
bool IsCoreBusy(int coreID)
{
    bool result;
    lk_lock(&run_queues[coreID].lk, 1);
    result = !run_queues[coreID].q.empty();
    lk_unlock(&run_queues[coreID].lk);
    return result;
}

/* ========================================================================== */
bool NeedsReschedule(int coreID)
{
    tick_t since_schedule = cores[coreID]->sim_cycle - run_queues[coreID].last_reschedule;
    return (knobs.scheduler_tick > 0) && (since_schedule > knobs.scheduler_tick);
}

/* ========================================================================== */
void SetThreadAffinity(pid_t tid, int coreID)
{
    assert(coreID >= 0 && coreID < num_cores);
    lk_lock(&affinity_lk, 1);
    //assert(affinity.count(tid) == 0);
    affinity[tid] = coreID;
    lk_unlock(&affinity_lk);
}

/* ========================================================================== */
static int GetThreadAffinity(pid_t tid)
{
    int res = INVALID_CORE;
    lk_lock(&affinity_lk, 1);
    if (affinity.count(tid) > 0)
        res = affinity[tid];
    lk_unlock(&affinity_lk);
    return res;
}

/* Helpers for updating SHM thread->core and core->thread maps, which is allowed only
 * by the scheduler. */
static void UpdateSHMCoreThread(int coreID, pid_t tid)
{
    lk_lock(lk_coreThreads, 1);
    coreThreads[coreID] = tid;
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

} // namespace xiosim
