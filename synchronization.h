
/* Synchronization primitives provided by feeder:
 * These are wrappers to pin internal threading functions.
 * Using pthreads inside a pintool is not safe.
 * Copyright, Svilen Kanev, 2011
 */

namespace LEVEL_BASE {
extern void GetLock(int32_t* lk, int32_t tid);
extern int32_t ReleaseLock(int32_t* lk);
extern void InitLock(int32_t* lk);
}

namespace LEVEL_PINCLIENT {
extern void PIN_Yield();
}

inline void lk_lock(int32_t* lk, int32_t cid)
{
    LEVEL_BASE::GetLock(lk, cid);
}

inline int32_t lk_unlock(int32_t* lk)
{
    return LEVEL_BASE::ReleaseLock(lk);
}

inline void lk_init(int32_t* lk)
{
    LEVEL_BASE::InitLock(lk);
}

inline void yield()
{
    LEVEL_PINCLIENT::PIN_Yield();
}

extern void spawn_new_thread(void entry_point(void*), void* arg);

/* Protecting shared state among cores */
/* Lock acquire order (outmost to inmost):
 - cache_lock
 - memory_lock
 - pool_lock (in oracle and core)
 */

/* Memory lock should be acquired before functional accesses to
 * the simulated application memory. */
extern int32_t memory_lock;
/* Cahche lock should be acquired before any access to the shared
 * caches (including enqueuing requests from lower-level caches). */
extern int32_t cache_lock;
