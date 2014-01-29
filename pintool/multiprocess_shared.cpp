#include <sstream>

#include "boost_interprocess.h"

#include "../interface.h"
#include "multiprocess_shared.h"
#include "ipc_queues.h"
#include "scheduler.h"


using namespace xiosim::shared;

boost::interprocess::managed_shared_memory *global_shm;

SHARED_VAR_DEFINE(XIOSIM_LOCK, printing_lock)
SHARED_VAR_DEFINE(int, num_processes)
SHARED_VAR_DEFINE(int, next_asid)

SHARED_VAR_DEFINE(pid_t, coreThreads)
SHARED_VAR_DEFINE(ThreadCoreMap, threadCores)
SHARED_VAR_DEFINE(XIOSIM_LOCK, lk_coreThreads)

SHARED_VAR_DEFINE(bool, sleeping_enabled)

SHARED_VAR_DEFINE(bool, producers_sleep)
SHARED_VAR_DEFINE(pthread_cond_t, cv_producers)
SHARED_VAR_DEFINE(pthread_mutex_t, cv_producers_lock)

SHARED_VAR_DEFINE(bool, consumers_sleep)
SHARED_VAR_DEFINE(pthread_cond_t, cv_consumers)
SHARED_VAR_DEFINE(pthread_mutex_t, cv_consumers_lock)

SHARED_VAR_DEFINE(int, ss_curr);
SHARED_VAR_DEFINE(int, ss_prev);

int InitSharedState(bool producer_process, pid_t harness_pid, int num_cores)
{
    using namespace boost::interprocess;
    int *process_counter = NULL;
    int asid = -1;

    std::stringstream harness_pid_stream;
    harness_pid_stream << harness_pid;
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

    process_counter = global_shm->find_or_construct<int>(counter_lock_key.c_str())();
    std::cout << getpid() << ": Counter value is: " << *process_counter << std::endl;
    (*process_counter)--;

    if (producer_process) {
        SHARED_VAR_INIT(int, next_asid);
        asid = *next_asid;
        (*next_asid)++;
    }

    InitIPCQueues();

    SHARED_VAR_INIT(bool, sleeping_enabled, false)

    SHARED_VAR_INIT(bool, producers_sleep, false)
    SHARED_VAR_INIT(pthread_cond_t, cv_producers);
    SHARED_VAR_INIT(pthread_mutex_t, cv_producers_lock);
    pthread_cond_init(cv_producers, NULL);
    pthread_mutex_init(cv_producers_lock, NULL);

    SHARED_VAR_INIT(bool, consumers_sleep, false)
    SHARED_VAR_INIT(pthread_cond_t, cv_consumers);
    SHARED_VAR_INIT(pthread_mutex_t, cv_consumers_lock);
    pthread_cond_init(cv_consumers, NULL);
    pthread_mutex_init(cv_consumers_lock, NULL);

    SHARED_VAR_ARRAY_INIT(pid_t, coreThreads, num_cores, xiosim::INVALID_THREADID);
    SHARED_VAR_CONSTRUCT(ThreadCoreMap, threadCores, shared_memory_key.c_str());
    SHARED_VAR_INIT(XIOSIM_LOCK, lk_coreThreads);

    SHARED_VAR_INIT(XIOSIM_LOCK, printing_lock);
    SHARED_VAR_INIT(int, num_processes);

    SHARED_VAR_INIT(int, ss_curr);
    SHARED_VAR_INIT(int, ss_prev);

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
    std::cout << getpid() << ": Proceeeding to execute." << std::endl;
    return asid;
}

pid_t GetSHMCoreThread(int coreID) {
    pid_t res;
    lk_lock(lk_coreThreads, 1);
    res = coreThreads[coreID];
    lk_unlock(lk_coreThreads);
    return res;
}

int GetSHMThreadCore(pid_t tid) {
    int res = INVALID_CORE;
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

void disable_producers()
{
    if (*sleeping_enabled) {
        pthread_mutex_lock(cv_producers_lock);
        *producers_sleep = true;
        pthread_mutex_unlock(cv_producers_lock);
    }
}

void enable_producers()
{
    pthread_mutex_lock(cv_producers_lock);
    *producers_sleep = false;
    pthread_cond_broadcast(cv_producers);
    pthread_mutex_unlock(cv_producers_lock);
}

void wait_producers()
{
    if (!*sleeping_enabled)
        return;

    pthread_mutex_lock(cv_producers_lock);

    while (*producers_sleep)
        pthread_cond_wait(cv_producers, cv_producers_lock);

    pthread_mutex_unlock(cv_producers_lock);
}

void disable_consumers()
{
    if (*sleeping_enabled) {
        pthread_mutex_lock(cv_consumers_lock);
        *consumers_sleep = true;
        pthread_mutex_unlock(cv_consumers_lock);
    }
}

void enable_consumers()
{
    pthread_mutex_lock(cv_consumers_lock);
    *consumers_sleep = false;
    pthread_cond_broadcast(cv_consumers);
    pthread_mutex_unlock(cv_consumers_lock);
}

void wait_consumers()
{
    if (!*sleeping_enabled)
        return;

    pthread_mutex_lock(cv_consumers_lock);

    while (*consumers_sleep)
        pthread_cond_wait(cv_consumers, cv_consumers_lock);

    pthread_mutex_unlock(cv_consumers_lock);
}
