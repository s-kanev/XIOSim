/*
 * Scheduling functions for mapping app threads to cores
 * Copyright, Svilen Kanev, 2013
 */


#include <queue>
#include <map>

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
void InitScheduler(int num_cores)
{
    run_queues = new RunQueue[num_cores];
    last_coreID = 0;
}

/* ========================================================================== */
void ScheduleNewThread(pid_t tid)
{
    if (GetSHMThreadCore(tid) != -1) {
        lk_lock(printing_lock, 1);
        std::cerr << "ScheduleNewThread: thread " << tid << " already scheduled, ignoring." << std::endl;
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

    last_coreID  = (last_coreID + 1) % num_cores;
}

/* ========================================================================== */
void DescheduleActiveThread(int coreID)
{
    lk_lock(&run_queues[coreID].lk, 1);

    pid_t tid = run_queues[coreID].q.front();
    run_queues[coreID].last_reschedule = cores[coreID]->sim_cycle;

    lk_lock(printing_lock, 1);
    std::cerr << "Descheduling thread " << tid << " at coreID  " << coreID << std::endl;
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
        std::cerr << "Thread " << new_tid << " going on core " << coreID << std::endl;
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
void GiveUpCore(int coreID, bool reschedule_thread)
{
    lk_lock(&run_queues[coreID].lk, 1);
    pid_t tid = run_queues[coreID].q.front();
    run_queues[coreID].last_reschedule = cores[coreID]->sim_cycle;

    lk_lock(printing_lock, tid+1);
    std::cerr << "Thread " << tid << " giving up on core " << coreID << std::endl;
    lk_unlock(printing_lock);

    run_queues[coreID].q.pop();

    pid_t new_tid = INVALID_THREADID;
    if (!run_queues[coreID].q.empty()) {
        new_tid = run_queues[coreID].q.front();

        lk_lock(printing_lock, tid+1);
        std::cerr << "Thread " << new_tid << " going on core " << coreID << std::endl;
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
        std::cerr << "Rescheduling " << tid << " on core " << coreID << std::endl;
        lk_unlock(printing_lock);
    }
    lk_unlock(&run_queues[coreID].lk);

    UpdateSHMRunqueues(coreID, new_tid);

    if (new_tid != tid)
        UpdateSHMThreadCore(tid, -1);
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

} // namespace xiosim
