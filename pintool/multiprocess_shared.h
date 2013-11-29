#ifndef __MP_SHARED__
#define __MP_SHARED__

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>

#include "mpkeys.h"

extern boost::interprocess::managed_shared_memory *global_shm;

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
    VAR = new TYPE(XIOSIM_SHARED_MEMORY_KEY, VAR##_KEY, ## __VA_ARGS__);

#include "shared_map.h"
#include "feeder.h"
#include "BufferManager.h"

SHARED_VAR_DECLARE(bool, consumers_sleep)
SHARED_VAR_DECLARE(bool, producers_sleep)
SHARED_VAR_DECLARE(bool, sleeping_enabled)
SHARED_VAR_DECLARE(BufferManager, handshake_buffer)

SHARED_VAR_DECLARE(PIN_SEMAPHORE, consumer_sleep_lock)
SHARED_VAR_DECLARE(PIN_SEMAPHORE, producer_sleep_lock)

/* Message queue form calling functions in timing_sim from feeder */
typedef boost::interprocess::deque<ipc_message_t> MessageQueue;
SHARED_VAR_DECLARE(MessageQueue, ipcMessageQueue);
SHARED_VAR_DECLARE(XIOSIM_LOCK, lk_ipcMessageQueue);

SHARED_VAR_DECLARE(THREADID, coreThreads);
typedef xiosim::shared::SharedMemoryMap<THREADID, int> ThreadCoreMap;
SHARED_VAR_DECLARE(ThreadCoreMap, threadCores);
SHARED_VAR_DECLARE(XIOSIM_LOCK, lk_coreThreads);
THREADID GetSHMRunqueue(int coreID);
bool GetSHMCoreBusy(int coreID);
int GetSHMThreadCore(THREADID tid);

SHARED_VAR_DECLARE(XIOSIM_LOCK, printing_lock);

void InitSharedState(bool wait_for_others);
void SendIPCMessage(ipc_message_t msg);

void disable_consumers();
void enable_consumers();
void disable_producers();
void enable_producers();
void wait_consumers();

extern KNOB<int> KnobNumProcesses;
extern KNOB<int> KnobNumCores;

#endif /* __MP_SHARED__ */
