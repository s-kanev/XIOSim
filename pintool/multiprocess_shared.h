#ifndef __MP_SHARED__
#define __MP_SHARED__

extern boost::interprocess::managed_shared_memory *global_shm;

#define Q(X) #X

#define SHARED_VAR_DECLARE(TYPE, VAR) \
    extern TYPE * VAR;   \
    extern const char* VAR##_KEY;

#define SHARED_VAR_DEFINE(TYPE, VAR) \
    TYPE * VAR;   \
    const char* VAR##_KEY = Q(VAR);

#define SHARED_VAR_INIT(TYPE, VAR, ...) \
    VAR = global_shm->find_or_construct<TYPE>(VAR##_KEY)(## __VA_ARGS__);

SHARED_VAR_DECLARE(bool, consumers_sleep)
SHARED_VAR_DECLARE(bool, producers_sleep)
SHARED_VAR_DECLARE(BufferManager, handshake_buffer)

SHARED_VAR_DECLARE(PIN_SEMAPHORE, consumer_sleep_lock);
SHARED_VAR_DECLARE(PIN_SEMAPHORE, producer_sleep_lock);

void InitSharedState(bool wait_for_others);

#endif /* __MP_SHARED__ */
