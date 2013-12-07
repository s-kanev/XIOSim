#include "shared_unordered_map.h"
#include "multiprocess_shared.h"
#include <sstream>
#include "ipc_queues.h"

using namespace xiosim::shared;

boost::interprocess::managed_shared_memory *global_shm;

SHARED_VAR_DEFINE(bool, consumers_sleep)
SHARED_VAR_DEFINE(bool, producers_sleep)
SHARED_VAR_DEFINE(bool, sleeping_enabled)
SHARED_VAR_DEFINE(PIN_SEMAPHORE, consumer_sleep_lock)
SHARED_VAR_DEFINE(PIN_SEMAPHORE, producer_sleep_lock)

SHARED_VAR_DEFINE(pid_t, coreThreads)
SHARED_VAR_DEFINE(ThreadCoreMap, threadCores)
SHARED_VAR_DEFINE(XIOSIM_LOCK, lk_coreThreads)

SHARED_VAR_DEFINE(XIOSIM_LOCK, printing_lock)

KNOB<int> KnobNumProcesses(KNOB_MODE_WRITEONCE,      "pintool",
        "num_processes", "1", "Number of processes for a multiprogrammed workload");
KNOB<int> KnobNumCores(KNOB_MODE_WRITEONCE,      "pintool",
        "num_cores", "1", "Number of cores simulated");
KNOB<pid_t> KnobHarnessPid(KNOB_MODE_WRITEONCE, "pintool",
        "harness_pid", "-1", "Process id of the harness process.");

void InitSharedState(bool wait_for_others, pid_t harness_pid)
{
    using namespace boost::interprocess;
    int *process_counter;
    std::stringstream harness_pid_stream;
    harness_pid_stream << KnobHarnessPid.Value();
    std::string shared_memory_key =
        harness_pid_stream.str() + std::string(XIOSIM_SHARED_MEMORY_KEY);
    std::string init_lock_key =
        harness_pid_stream.str() + std::string(XIOSIM_INIT_SHARED_LOCK);
    std::string counter_lock_key =
        harness_pid_stream.str() + std::string(XIOSIM_INIT_COUNTER_KEY);

    std::cout << getpid() << ": About to init pid " << std::endl;
    std::cout << "lock key is " << init_lock_key << std::endl;

    named_mutex init_lock(open_only, init_lock_key.c_str());
    global_shm = new managed_shared_memory(open_or_create,
        shared_memory_key.c_str(), DEFAULT_SHARED_MEMORY_SIZE);
    init_lock.lock();

    if (wait_for_others) {
        process_counter = global_shm->find_or_construct<int>(counter_lock_key.c_str())();
        std::cout << getpid() << ": Counter value is: " << *process_counter << std::endl;
        (*process_counter)--;
    }

    InitIPCQueues();

    SHARED_VAR_INIT(bool, producers_sleep, false)
    SHARED_VAR_INIT(bool, consumers_sleep, false)
    SHARED_VAR_INIT(bool, sleeping_enabled)
    SHARED_VAR_INIT(PIN_SEMAPHORE, consumer_sleep_lock);
    SHARED_VAR_INIT(PIN_SEMAPHORE, producer_sleep_lock);
    PIN_SemaphoreInit(consumer_sleep_lock);
    PIN_SemaphoreInit(producer_sleep_lock);

    SHARED_VAR_ARRAY_INIT(pid_t, coreThreads, KnobNumCores.Value(), INVALID_THREADID);
    SHARED_VAR_CONSTRUCT(ThreadCoreMap, threadCores, shared_memory_key.c_str());
    SHARED_VAR_INIT(XIOSIM_LOCK, lk_coreThreads);

    SHARED_VAR_INIT(XIOSIM_LOCK, printing_lock);

    init_lock.unlock();

    if (wait_for_others) {
        // Spin until the counter reaches zero, indicating that all other processes
        // have reached this point.
        while (1) {
            init_lock.lock();
            if (*process_counter == 0) {
                init_lock.unlock();
                break;
            }
            init_lock.unlock();
        }
    }
    std::cout << getpid() << ": Proceeeding to execute pintool.\n";
}

pid_t GetSHMRunqueue(int coreID) {
    pid_t res;
    lk_lock(lk_coreThreads, 1);
    res = coreThreads[coreID];
    lk_unlock(lk_coreThreads);
    return res;
}

int GetSHMThreadCore(pid_t tid) {
   int res = -1;
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

VOID disable_consumers()
{
  if(*sleeping_enabled) {
    if(!*consumers_sleep) {
      PIN_SemaphoreClear(consumer_sleep_lock);
    }
    *consumers_sleep = true;
  }
}

VOID disable_producers()
{
  if(*sleeping_enabled) {
    if(!*producers_sleep) {
      PIN_SemaphoreClear(producer_sleep_lock);
    }
    *producers_sleep = true;
  }
}

VOID enable_consumers()
{
  if(*consumers_sleep) {
    PIN_SemaphoreSet(consumer_sleep_lock);
  }
  *consumers_sleep = false;
}

VOID enable_producers()
{
  if(*producers_sleep) {
    PIN_SemaphoreSet(producer_sleep_lock);
  }
  *producers_sleep = false;
}

void wait_consumers()
{
  PIN_SemaphoreWait(consumer_sleep_lock);
}
