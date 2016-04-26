/*
 * Scheduling functions for mapping app threads to cores
 * Copyright, Svilen Kanev, 2013
 */

#include <queue>
#include <map>
#include <mutex>
#include <list>
#include <vector>

#include "multiprocess_shared.h"

#include "xiosim/core_const.h"
#include "xiosim/libsim.h"
#include "xiosim/sim.h"
#include "xiosim/synchronization.h"
#include "xiosim/zesto-core.h"
#include "xiosim/zesto-structs.h"

#include "scheduler.h"

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

static std::vector<RunQueue> run_queues;
static int num_cores;

static int last_coreID;
static XIOSIM_LOCK last_coreID_lk;

static std::map<pid_t, int> affinity;
static XIOSIM_LOCK affinity_lk;
static int GetThreadAffinity(pid_t tid);

static void UpdateSHMCoreThread(int coreID, pid_t tid);
static void UpdateSHMThreadCore(pid_t tid, int coreID);
static void RemoveSHMThread(pid_t tid);

struct TCB {
    pid_t tid;

    /* tid of a thread we're trying to join on.
     * When blocked_on != INVALID_THREADID, the thread still sits on a run queue,
     * but gets shuffled back if it reaches the head. So all is good and fair.
     * XXX: This is a prototype of a more general blocking mechanism where we
     * can block on an arbitrary memory address (for sys_futex) or file descriptor
     * (for sys_epoll and the like).
     */
    pid_t blocked_on;

    /* Which run queue is this thread sitting on.
     * This is different from threadCores[tid]. threadCores is INVALID_CORE,
     * unless the thread is actually running on a core, while run_queue_ID is set
     * while the thread is waiting on a queue as well. */
    int run_queue_ID;

    TCB(pid_t tid)
        : tid(tid)
        , blocked_on(INVALID_THREADID)
        , run_queue_ID(INVALID_CORE) {}
};
static std::map<pid_t, TCB> threads;
static XIOSIM_LOCK tcb_lk;

/* From blocker to blocked */
static std::map<pid_t, pid_t> blocked_thread_map;
static XIOSIM_LOCK blocked_threads_lk;
static bool IsThreadBlocked(pid_t);
static void UnblockSleepers(pid_t tid);

/* ========================================================================== */
void InitScheduler(int num_cores_) {
    num_cores = num_cores_;
    run_queues.resize(num_cores);

    last_coreID = 0;
    lk_init(&last_coreID_lk);

    lk_init(&affinity_lk);
}

/* ========================================================================== */
int ScheduleNewThread(pid_t tid) {
    /* Make sure thread is queued at one and only one runqueue. */
    if (IsSHMThreadSimulatingMaybe(tid)) {
#ifdef SCHEDULER_DEBUG
        lk_lock(printing_lock, 1);
        std::cerr << "ScheduleNewThread: thread " << tid << " already scheduled, ignoring."
                  << std::endl;
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
        last_coreID = (last_coreID + 1) % num_cores;
        lk_unlock(&last_coreID_lk);
    }

    {
        /* We know what runQ we'll sit on, set the TCB field
           (and create a TCB if it's our first time around). */
        std::lock_guard<XIOSIM_LOCK> l(tcb_lk);
        if (threads.count(tid) == 0)
            threads.insert(std::make_pair(tid, tid));
        threads.at(tid).run_queue_ID = coreID;
    }

    lk_lock(&run_queues[coreID].lk, 1);
    if (run_queues[coreID].q.empty() && !xiosim::libsim::is_core_active(coreID)) {
        /* We can go on @coreID straight away, activate it and update SHM state. */
        xiosim::libsim::activate_core(coreID);
        UpdateSHMThreadCore(tid, coreID);
        UpdateSHMCoreThread(coreID, tid);
    } else {
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

    /* TODO: UpdateProcessCoreSet if not called from ScheduleProcessThreads,
     * which can happen on a newly created thread during a slice. */
}

/* Threads are ordered by affinity to "virtual cores".
 * We actually schedule them to real cores (which could be different from the
 * virtual ones, but we preserve the ordering between @threads). */
/* ========================================================================== */
void ScheduleProcessThreads(int asid, std::list<pid_t> threads) {
    CoreSet scheduled_cores;

    /* XXX: Hardcoded policy for now, each process gets a
     * hardcoded contiguous subset of cores. */
    int offset = asid * (num_cores / *num_processes);

    int i = 0;
    for (pid_t tid : threads) {
        int coreID = offset + i;
        assert(coreID < num_cores);
        SetThreadAffinity(tid, coreID);
        scheduled_cores.insert(coreID);
        i++;
    }

    UpdateProcessCoreSet(asid, scheduled_cores);

    for (pid_t tid : threads) {
        ScheduleNewThread(tid);
    }
}

/* Helper to shuffle blocked threads on the front of the runQ to the back.
 * Assumes the caller is holding run_queues[@coreID].lk */
static void RequeueSleepers(int coreID) {
    if (run_queues[coreID].q.empty())
        return;

    pid_t curr_head;
    pid_t first_tid = run_queues[coreID].q.front();

    while (true) {
        curr_head = run_queues[coreID].q.front();

        /* Found an unblocked thread, we're done. */
        if (!IsThreadBlocked(curr_head))
            return;

        /* Else, a blocked thread gives up its slot. And we are still fair. */
        run_queues[coreID].q.pop();
        run_queues[coreID].q.push(curr_head);

        /* We've gone a full round around the runQ and found nothing. */
        if (first_tid == run_queues[coreID].q.front()) {
            return;
        }
    }
}

/* Helper to get the first non-blocked thread on the requested runQ.
 * Assumes the caller is holding run_queues[@coreID].lk
 * If there are "non-real", i.e. blocked, threads at the head of the runQ,
 * they will get shuffled to the back until we either find a non-blocked one,
 * or we go full circle. */
static pid_t GetRealRunQHead(int coreID) {
    if (run_queues[coreID].q.empty())
        return INVALID_THREADID;

    RequeueSleepers(coreID);

    pid_t new_head = run_queues[coreID].q.front();
    if (IsThreadBlocked(new_head))
        return INVALID_THREADID;

    return new_head;
}

/* Helper to schedule the next thread, if any, to an already activated core.
 * Skips all blocked thread on the runQ.
 * Assumes the caller is holding run_queues[@coreID].lk */
static void ScheduleNextUnblockedThread(int coreID) {
    pid_t new_tid = GetRealRunQHead(coreID);

    if (new_tid != INVALID_THREADID) {
        /* Let SHM know @new_tid is running on @coreID */
        UpdateSHMThreadCore(new_tid, coreID);

#ifdef SCHEDULER_DEBUG
        lk_lock(printing_lock, 1);
        std::cerr << "Thread " << new_tid << " going on core " << coreID << std::endl;
        lk_unlock(printing_lock);
#endif
    } else {
        /* No more work to do, let other cores progress */
        xiosim::libsim::deactivate_core(coreID);
    }

    /* Let SHM know @coreID has @new_tid (potentially nothing) */
    UpdateSHMCoreThread(coreID, new_tid);
}

/* ========================================================================== */
void DescheduleActiveThread(int coreID) {
    lk_lock(&run_queues[coreID].lk, 1);

    pid_t tid = run_queues[coreID].q.front();
    run_queues[coreID].last_reschedule = cores[coreID]->sim_cycle;

#ifdef SCHEDULER_DEBUG
    lk_lock(printing_lock, 1);
    std::cerr << "Descheduling thread " << tid << " at coreID  " << coreID << std::endl;
    lk_unlock(printing_lock);
#endif

    /* This thread is no more. */
    run_queues[coreID].q.pop();
    RemoveSHMThread(tid);
    {
        std::lock_guard<XIOSIM_LOCK> l(tcb_lk);
        threads.erase(tid);
    }

    /* Schedule the next unblocked thread from this runQ, if any. */
    ScheduleNextUnblockedThread(coreID);
    lk_unlock(&run_queues[coreID].lk);

    /* Threads that were waiting to join on this one are now good to go */
    UnblockSleepers(tid);
}

/* ========================================================================== */
/* XXX: This is called from a sim thread -- the one that frees up the core */
void GiveUpCore(int coreID, bool reschedule_thread) {
    lk_lock(&run_queues[coreID].lk, 1);
    pid_t tid = run_queues[coreID].q.front();
    run_queues[coreID].last_reschedule = cores[coreID]->sim_cycle;

#ifdef SCHEDULER_DEBUG
    lk_lock(printing_lock, tid + 1);
    std::cerr << "Thread " << tid << " giving up on core " << coreID << std::endl;
    lk_unlock(printing_lock);
#endif

    /* This thread gets descheduled. */
    run_queues[coreID].q.pop();

    pid_t new_tid = GetRealRunQHead(coreID);
    if (new_tid != INVALID_THREADID) {
        /* There is another thread waiting for the core. It gets scheduled. */

#ifdef SCHEDULER_DEBUG
        lk_lock(printing_lock, tid + 1);
        std::cerr << "Thread " << new_tid << " going on core " << coreID << std::endl;
        lk_unlock(printing_lock);
#endif
    } else if (!reschedule_thread) {
        /* No more work to do, let core sleep */
        xiosim::libsim::deactivate_core(coreID);
        new_tid = INVALID_THREADID;
    } else if (IsThreadBlocked(tid)) {
        /* Thread is the only one waiting for this core, but also blocked.
         * Same as an empty queue. */
        xiosim::libsim::deactivate_core(coreID);
        new_tid = INVALID_THREADID;
    } else {
        /* Thread is the only one waiting for this core, reschedule is moot. */
        new_tid = tid;
    }
    /* Let SHM know @coreID has @new_tid (potentially nothing) */
    UpdateSHMCoreThread(coreID, new_tid);
    if (new_tid != tid && new_tid != INVALID_THREADID)
        UpdateSHMThreadCore(new_tid, coreID);

    if (reschedule_thread) {
        /* Reschedule at the back of (possibly empty) runqueue for @coreID. */
        run_queues[coreID].q.push(tid);

#ifdef SCHEDULER_DEBUG
        lk_lock(printing_lock, tid + 1);
        std::cerr << "Rescheduling " << tid << " on core " << coreID << std::endl;
        lk_unlock(printing_lock);
#endif

        /* If old thread is requeued behind a new thread, update old's SHM status. */
        if (new_tid != tid && new_tid != INVALID_THREADID) {
            UpdateSHMThreadCore(tid, INVALID_CORE);
        }
    } else {
        /* For all we know in this case, old thread will never be scheduled again.
         */
        RemoveSHMThread(tid);
        {
            std::lock_guard<XIOSIM_LOCK> l(tcb_lk);
            threads.at(tid).run_queue_ID = INVALID_CORE;
        }
    }

    lk_unlock(&run_queues[coreID].lk);
}

/* ========================================================================== */
pid_t GetCoreThread(int coreID) {
    pid_t result;
    lk_lock(&run_queues[coreID].lk, 1);
    if (run_queues[coreID].q.empty())
        result = INVALID_THREADID;
    else {
        result = run_queues[coreID].q.front();
        if (IsThreadBlocked(result))
            result = INVALID_THREADID;
    }
    lk_unlock(&run_queues[coreID].lk);
    return result;
}

/* ========================================================================== */
bool IsCoreBusy(int coreID) {
    bool result;
    lk_lock(&run_queues[coreID].lk, 1);
    result = !run_queues[coreID].q.empty();
    lk_unlock(&run_queues[coreID].lk);
    return result;
}

/* ========================================================================== */
bool NeedsReschedule(int coreID) {
    tick_t since_schedule = cores[coreID]->sim_cycle - run_queues[coreID].last_reschedule;
    return (system_knobs.scheduler_tick > 0) && (since_schedule > system_knobs.scheduler_tick);
}

/* ========================================================================== */
void SetThreadAffinity(pid_t tid, int coreID) {
    assert(coreID >= 0 && coreID < num_cores);
    lk_lock(&affinity_lk, 1);
    // assert(affinity.count(tid) == 0);
    affinity[tid] = coreID;
    lk_unlock(&affinity_lk);
}

/* ========================================================================== */
static int GetThreadAffinity(pid_t tid) {
    int res = INVALID_CORE;
    lk_lock(&affinity_lk, 1);
    if (affinity.count(tid) > 0)
        res = affinity[tid];
    lk_unlock(&affinity_lk);
    return res;
}

/* Helpers for updating SHM thread->core and core->thread maps, which is allowed
 * only by the scheduler. */
static void UpdateSHMCoreThread(int coreID, pid_t tid) {
    lk_lock(lk_coreThreads, 1);
    coreThreads[coreID] = tid;
    lk_unlock(lk_coreThreads);
}

static void UpdateSHMThreadCore(pid_t tid, int coreID) {
    if (tid == INVALID_THREADID)
        return;
    lk_lock(lk_coreThreads, 1);
    threadCores->operator[](tid) = coreID;
    lk_unlock(lk_coreThreads);
}

static void RemoveSHMThread(pid_t tid) {
    lk_lock(lk_coreThreads, 1);
    threadCores->erase(tid);
    lk_unlock(lk_coreThreads);
}

static bool IsThreadBlocked(pid_t tid) {
    std::lock_guard<XIOSIM_LOCK> l(tcb_lk);
    TCB& tcb = threads.at(tid);
    return tcb.blocked_on != INVALID_THREADID;
}

void BlockThread(int coreID, pid_t tid, pid_t blocked_on) {
    {
        std::lock_guard<XIOSIM_LOCK> l(tcb_lk);
        TCB& tcb = threads.at(tid);
        tcb.blocked_on = blocked_on;
    }
    {
        std::lock_guard<XIOSIM_LOCK> l(blocked_threads_lk);
        // XXX: For now, we'll have a proper list of waiters soon.
        assert(blocked_thread_map.count(blocked_on) == 0);
        blocked_thread_map[blocked_on] = tid;
    }

#ifdef SCHEDULER_DEBUG
    {
        std::lock_guard<XIOSIM_LOCK> l(*printing_lock);
        std::cerr << "Thread " << tid << " joining " << blocked_on << " on core " << coreID << std::endl;
    }
#endif
    GiveUpCore(coreID, true);
}

void UnblockSleepers(pid_t tid) {
    pid_t blockee;
    {
        std::lock_guard<XIOSIM_LOCK> l(blocked_threads_lk);
        if (blocked_thread_map.count(tid) == 0)
            return;

        blockee = blocked_thread_map[tid];
        blocked_thread_map.erase(tid);
    }
    int run_queue_ID;
    {
        std::lock_guard<XIOSIM_LOCK> l(tcb_lk);
        TCB& tcb = threads.at(blockee);
        assert(tcb.blocked_on = tid);
        tcb.blocked_on = INVALID_THREADID;
        run_queue_ID = tcb.run_queue_ID;
    }
    if (run_queue_ID == INVALID_CORE)
        return;
    {
        std::lock_guard<XIOSIM_LOCK> l(run_queues[run_queue_ID].lk);
        /* If the thread we're waking up is at the head of its runQ, the runQ is
         * effectively empty. We need to wake the core up and schedule the thread.
         * In all other cases, all will be well when the thread's turn comes. */
        if (run_queues[run_queue_ID].q.front() == blockee) {
            xiosim::libsim::activate_core(run_queue_ID);
            UpdateSHMThreadCore(blockee, run_queue_ID);
            UpdateSHMCoreThread(run_queue_ID, blockee);
        }
    }
}
}  // namespace xiosim
