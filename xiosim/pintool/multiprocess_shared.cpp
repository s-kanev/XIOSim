#include <sstream>

#include "xiosim/core_const.h"

#include "ipc_queues.h"
#include "scheduler.h"

#include "multiprocess_shared.h"

using namespace xiosim::shared;

boost::interprocess::managed_shared_memory* global_shm;

SHARED_VAR_DEFINE(XIOSIM_LOCK, printing_lock)
SHARED_VAR_DEFINE(int, num_processes)
SHARED_VAR_DEFINE(int, next_asid)
SHARED_VAR_DEFINE(time_t, feeder_watchdogs)

SHARED_VAR_DEFINE(int, num_done_fastforward)
SHARED_VAR_DEFINE(int, fastforward_epoch)
SHARED_VAR_DEFINE(XIOSIM_LOCK, lk_num_done_fastforward)

SHARED_VAR_DEFINE(int, num_done_slice)
SHARED_VAR_DEFINE(int, slice_epoch)
SHARED_VAR_DEFINE(XIOSIM_LOCK, lk_num_done_slice)

SHARED_VAR_DEFINE(double, global_sim_time)
SHARED_VAR_DEFINE(int64_t, timestamp_counters)

SHARED_VAR_DEFINE(ThreadProcessMap, threadProcess)
SHARED_VAR_DEFINE(XIOSIM_LOCK, lk_threadProcess)

SHARED_VAR_DEFINE(pid_t, coreThreads)
SHARED_VAR_DEFINE(ThreadCoreMap, threadCores)
SHARED_VAR_DEFINE(XIOSIM_LOCK, lk_coreThreads)

SHARED_VAR_DEFINE(SharedCoreSetArray, processCoreSet)
SHARED_VAR_DEFINE(XIOSIM_LOCK, lk_processCoreSet)

SHARED_VAR_DEFINE(SharedCoreAllocation, coreAllocation)
SHARED_VAR_DEFINE(XIOSIM_LOCK, lk_coreAllocation)

SHARED_VAR_DEFINE(ThreadBOSMap, thread_bos)
SHARED_VAR_DEFINE(XIOSIM_LOCK, lk_thread_bos)

SHARED_VAR_DEFINE(bool, sleeping_enabled)

SHARED_VAR_DEFINE(bool, waits_as_loads);
SHARED_VAR_DEFINE(int, ss_curr);
SHARED_VAR_DEFINE(int, ss_prev);

namespace xiosim {
namespace shared {
static int num_cores;
}  // xiosim::shared
}  // xiosim

static shared_core_set_allocator* core_set_alloc_inst;
static int_allocator* int_alloc_inst;

int InitSharedState(bool producer_process, pid_t harness_pid, int num_cores_) {
    using namespace boost::interprocess;
    int* process_counter = NULL;
    int asid = -1;

    std::stringstream harness_pid_stream;
    harness_pid_stream << harness_pid;
    std::string shared_memory_key =
        harness_pid_stream.str() + std::string(XIOSIM_SHARED_MEMORY_KEY);
    std::string init_lock_key = harness_pid_stream.str() + std::string(XIOSIM_INIT_SHARED_LOCK);
    std::string counter_lock_key = harness_pid_stream.str() + std::string(XIOSIM_INIT_COUNTER_KEY);

#ifdef MP_DEBUG
    std::cout << getpid() << ": About to init pid " << std::endl;
    std::cout << "lock key is " << init_lock_key << std::endl;
#endif

    named_mutex init_lock(open_only, init_lock_key.c_str());
    global_shm = new managed_shared_memory(
        open_or_create, shared_memory_key.c_str(), DEFAULT_SHARED_MEMORY_SIZE);
    init_lock.lock();

    process_counter = global_shm->find_or_construct<int>(counter_lock_key.c_str())();
#ifdef MP_DEBUG
    std::cout << getpid() << ": Counter value is: " << *process_counter << std::endl;
#endif
    (*process_counter)--;

    if (producer_process) {
        SHARED_VAR_INIT(int, next_asid);
        asid = *next_asid;
        (*next_asid)++;
    }

    InitIPCQueues();

    xiosim::shared::num_cores = num_cores_;

    SHARED_VAR_INIT(bool, sleeping_enabled, false)

    SHARED_VAR_ARRAY_INIT(pid_t, coreThreads, xiosim::shared::num_cores, xiosim::INVALID_THREADID);
    SHARED_VAR_CONSTRUCT(ThreadCoreMap, threadCores);
    SHARED_VAR_INIT(XIOSIM_LOCK, lk_coreThreads);
    lk_init(lk_coreThreads);

    SHARED_VAR_CONSTRUCT(ThreadBOSMap, thread_bos);
    SHARED_VAR_INIT(XIOSIM_LOCK, lk_thread_bos);
    lk_init(lk_thread_bos);

    SHARED_VAR_CONSTRUCT(ThreadProcessMap, threadProcess);
    SHARED_VAR_INIT(XIOSIM_LOCK, lk_threadProcess);
    lk_init(lk_threadProcess);

    SHARED_VAR_CONSTRUCT(SharedCoreAllocation, coreAllocation);
    SHARED_VAR_INIT(XIOSIM_LOCK, lk_coreAllocation)
    lk_init(lk_coreAllocation);

    SHARED_VAR_INIT(XIOSIM_LOCK, printing_lock);
    SHARED_VAR_INIT(int, num_processes);
    SHARED_VAR_INIT(time_t, feeder_watchdogs);

    SHARED_VAR_INIT(int, num_done_fastforward, 0);
    SHARED_VAR_INIT(int, fastforward_epoch, 0);
    SHARED_VAR_INIT(XIOSIM_LOCK, lk_num_done_fastforward);
    lk_init(lk_num_done_fastforward);

    SHARED_VAR_INIT(int, num_done_slice, 0);
    SHARED_VAR_INIT(int, slice_epoch, 0);
    SHARED_VAR_INIT(XIOSIM_LOCK, lk_num_done_slice);
    lk_init(lk_num_done_slice);

    SHARED_VAR_INIT(double, global_sim_time, 0);
    SHARED_VAR_ARRAY_INIT(int64_t, timestamp_counters, xiosim::shared::num_cores, 0);

    int_alloc_inst = new int_allocator(global_shm->get_segment_manager());
    core_set_alloc_inst = new shared_core_set_allocator(global_shm->get_segment_manager());
    SHARED_VAR_INIT(SharedCoreSetArray,
                    processCoreSet,
                    *num_processes,
                    SharedCoreSet(std::less<int>(), *int_alloc_inst),
                    *core_set_alloc_inst);
    SHARED_VAR_INIT(XIOSIM_LOCK, lk_processCoreSet);
    lk_init(lk_processCoreSet);


    SHARED_VAR_INIT(int, ss_curr);
    SHARED_VAR_INIT(int, ss_prev);
    SHARED_VAR_INIT(bool, waits_as_loads);

    init_lock.unlock();

    /* Spin until the counter reaches zero, indicating that all other processes
     * have reached this point. */
    while (1) {
        init_lock.lock();
        if (*process_counter == 0) {
            init_lock.unlock();
            break;
        }
        init_lock.unlock();
    }
#ifdef MP_DEBUG
    std::cout << getpid() << ": Proceeeding to execute." << std::endl;
#endif
    return asid;
}

void DeinitSharedState() {
    DeinitIPCQueues();

    delete core_set_alloc_inst;
    delete int_alloc_inst;
    delete global_shm;
}

pid_t GetSHMCoreThread(int coreID) {
    pid_t res;
    lk_lock(lk_coreThreads, 1);
    res = coreThreads[coreID];
    lk_unlock(lk_coreThreads);
    return res;
}

int GetSHMThreadCore(pid_t tid) {
    int res = xiosim::INVALID_CORE;
    lk_lock(lk_coreThreads, 1);
    if (threadCores->find(tid) != threadCores->end())
        res = threadCores->operator[](tid);
    lk_unlock(lk_coreThreads);
    return res;
}

bool IsSHMThreadSimulatingMaybe(pid_t tid) {
    bool res = false;
    lk_lock(lk_coreThreads, 1);
    if (threadCores->find(tid) != threadCores->end())
        res = true;
    lk_unlock(lk_coreThreads);
    return res;
}

CoreSet GetProcessCores(int asid) {
    CoreSet res;
    for (int coreID = 0; coreID < xiosim::shared::num_cores; coreID++) {
        pid_t tid = GetSHMCoreThread(coreID);
        if (tid == xiosim::INVALID_THREADID)
            continue;

        lk_lock(lk_threadProcess, 1);
        if (threadProcess->find(tid) != threadProcess->end() &&
            threadProcess->operator[](tid) == asid)
            res.insert(coreID);
        lk_unlock(lk_threadProcess);
    }
    return res;
}

void UpdateProcessCoreSet(int asid, CoreSet val) {
    lk_lock(lk_processCoreSet, 1);
    processCoreSet->at(asid).clear();
    for (int i : val)
        processCoreSet->at(asid).insert(i);
    lk_unlock(lk_processCoreSet);
}

CoreSet GetProcessCoreSet(int asid) {
    CoreSet res;
    lk_lock(lk_processCoreSet, 1);
    for (int i : processCoreSet->at(asid))
        res.insert(i);
    lk_unlock(lk_processCoreSet);
    return res;
}

void UpdateProcessCoreAllocation(int asid, int allocated_cores) {
    lk_lock(lk_coreAllocation, 1);
    coreAllocation->operator[](asid) = allocated_cores;
    lk_unlock(lk_coreAllocation);
}

int GetProcessCoreAllocation(int asid) {
    int res = 0;
    lk_lock(lk_coreAllocation, 1);
    if (coreAllocation->find(asid) != coreAllocation->end())
        res = coreAllocation->at(asid);
    lk_unlock(lk_coreAllocation);
    return res;
}
