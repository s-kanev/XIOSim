#ifndef __MP_SHARED__
#define __MP_SHARED__

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

#define SHARED_VAR_CONSTRUCT(TYPE, VAR, SHM_KEY, ...) \
    VAR = new TYPE(SHM_KEY, VAR##_KEY, ## __VA_ARGS__);

SHARED_VAR_DECLARE(bool, sleeping_enabled)

SHARED_VAR_DECLARE(bool, consumers_sleep)
SHARED_VAR_DECLARE(pthread_cond_t, cv_consumers)
SHARED_VAR_DECLARE(pthread_mutex_t, cv_consumers_lock)

SHARED_VAR_DECLARE(bool, producers_sleep)
SHARED_VAR_DECLARE(pthread_cond_t, cv_consumers)
SHARED_VAR_DECLARE(pthread_mutex_t, cv_consumers_lock)

SHARED_VAR_DECLARE(pid_t, coreThreads);
typedef xiosim::shared::SharedMemoryMap<pid_t, int> ThreadCoreMap;

SHARED_VAR_DECLARE(ThreadCoreMap, threadCores);
SHARED_VAR_DECLARE(XIOSIM_LOCK, lk_coreThreads);
pid_t GetSHMRunqueue(int coreID);
/* Will this thread ever get simulated (now or in the future) */
bool IsSHMThreadSimulatingMaybe(pid_t tid);
int GetSHMThreadCore(pid_t tid);

SHARED_VAR_DECLARE(int, num_processes);
SHARED_VAR_DECLARE(XIOSIM_LOCK, printing_lock);

/* librepeater */
SHARED_VAR_DECLARE(int, ss_curr);
SHARED_VAR_DECLARE(int, ss_prev);

/* Init state in shared memory. Returns unique address space id for producers */
int InitSharedState(bool producer_process, pid_t harness_pid, int num_cores);
void SendIPCMessage(ipc_message_t msg);

void disable_consumers();
void enable_consumers();
void wait_consumers();
void disable_producers();
void enable_producers();
void wait_producers();

#endif /* __MP_SHARED__ */
