#ifndef __MP_SHARED__
#define __MP_SHARED__

#include "../core-set.h"
#include "mpkeys.h"

extern boost::interprocess::managed_shared_memory *global_shm;

/* Returns "X" */
#define Q(X) #X
#define SINGLE_ARG(...) __VA_ARGS__

#define SHARED_VAR_DECLARE(TYPE, VAR) \
    extern TYPE * VAR;   \
    extern const char* VAR##_KEY;

#define SHARED_VAR_DEFINE(TYPE, VAR) \
    TYPE * VAR;   \
    const char* VAR##_KEY = Q(VAR);

#define SHARED_VAR_INIT(TYPE, VAR, ...) \
    VAR = global_shm->find_or_construct<TYPE>(VAR##_KEY)( __VA_ARGS__);

#define SHARED_VAR_ARRAY_INIT(TYPE, VAR, SIZE, ...) \
    VAR = global_shm->find_or_construct<TYPE>(VAR##_KEY)[SIZE]( __VA_ARGS__);

#define SHARED_VAR_CONSTRUCT(TYPE, VAR, ...) \
    VAR = new TYPE(global_shm, VAR##_KEY, ## __VA_ARGS__);

SHARED_VAR_DECLARE(int, num_processes)
SHARED_VAR_DECLARE(int, next_asid)
SHARED_VAR_DECLARE(XIOSIM_LOCK, printing_lock)

typedef xiosim::shared::SharedMemoryMap<pid_t, int> ThreadProcessMap;
SHARED_VAR_DECLARE(ThreadProcessMap, threadProcess)
SHARED_VAR_DECLARE(XIOSIM_LOCK, lk_threadProcess)

SHARED_VAR_DECLARE(pid_t, coreThreads);
typedef xiosim::shared::SharedMemoryMap<pid_t, int> ThreadCoreMap;

SHARED_VAR_DECLARE(ThreadCoreMap, threadCores);
SHARED_VAR_DECLARE(XIOSIM_LOCK, lk_coreThreads);
/* Get the thread currently executing on core @coreID. Returns INVALID_THREADID if none */
pid_t GetSHMCoreThread(int coreID);
/* Will this thread ever get simulated (now or in the future) */
bool IsSHMThreadSimulatingMaybe(pid_t tid);
/* Get the core that this thread is executing on.
 * Returning INVALID_CORE means either: (i) the thread is waiting on
 * a core runqueue and will get executed, or (ii) it has never been
 * scheduled. Calling IsSHMThreadSimulatingMaybe() can distinguish
 * between the two. */
int GetSHMThreadCore(pid_t tid);

typedef boost::interprocess::allocator<int, boost::interprocess::managed_shared_memory::segment_manager> int_allocator; 
typedef boost::interprocess::set<int, std::less<int>, int_allocator> SharedCoreSet;
typedef boost::interprocess::allocator<SharedCoreSet, boost::interprocess::managed_shared_memory::segment_manager> shared_core_set_allocator;
typedef boost::interprocess::vector<SharedCoreSet, shared_core_set_allocator> SharedCoreSetArray;
SHARED_VAR_DECLARE(SharedCoreSetArray, processCoreSet)
SHARED_VAR_DECLARE(XIOSIM_LOCK, lk_processCoreSet)
CoreSet GetProcessCores(int asid);
CoreSet GetProcessCoreSet(int asid);
void UpdateProcessCoreSet(int asid, CoreSet val);

typedef xiosim::shared::SharedMemoryMap<int, int> SharedCoreAllocation;
SHARED_VAR_DECLARE(SharedCoreAllocation, coreAllocation)
SHARED_VAR_DECLARE(XIOSIM_LOCK, lk_coreAllocation)
int GetProcessCoreAllocation(int asid);
void UpdateProcessCoreAllocation(int asid, int allocated_cores);

typedef xiosim::shared::SharedMemoryMap<pid_t, md_addr_t> ThreadBOSMap;
SHARED_VAR_DECLARE(ThreadBOSMap, thread_bos)
SHARED_VAR_DECLARE(XIOSIM_LOCK, lk_thread_bos)

SHARED_VAR_DECLARE(bool, sleeping_enabled)

SHARED_VAR_DECLARE(bool, consumers_sleep)
SHARED_VAR_DECLARE(pthread_cond_t, cv_consumers)
SHARED_VAR_DECLARE(pthread_mutex_t, cv_consumers_lock)

SHARED_VAR_DECLARE(bool, producers_sleep)
SHARED_VAR_DECLARE(pthread_cond_t, cv_consumers)
SHARED_VAR_DECLARE(pthread_mutex_t, cv_consumers_lock)

/* librepeater */
SHARED_VAR_DECLARE(int, ss_curr);
SHARED_VAR_DECLARE(int, ss_prev);

/* Init state in shared memory. Returns unique address space id for producers */
int InitSharedState(bool producer_process, pid_t harness_pid, int num_cores);

void disable_consumers();
void enable_consumers();
void wait_consumers();
void disable_producers();
void enable_producers();
void wait_producers();

#endif /* __MP_SHARED__ */
