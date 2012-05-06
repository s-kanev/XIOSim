
/* Synchronization primitives provided by feeder:
 * These are wrappers to pin internal threading functions.
 * Using pthreads inside a pintool is not safe.
 * Copyright, Svilen Kanev, 2011
 */


#ifndef __SYNCHRONIZATION_H__
#define __SYNCHRONIZATION_H__

namespace LEVEL_BASE {
struct PIN_LOCK;
extern void GetLock(PIN_LOCK* lk, int32_t tid);
extern int32_t ReleaseLock(PIN_LOCK* lk);
extern void InitLock(PIN_LOCK* lk);
}

namespace LEVEL_PINCLIENT {
extern void PIN_Yield();
}

inline void lk_lock(LEVEL_BASE::PIN_LOCK* lk, int32_t cid)
{
    LEVEL_BASE::GetLock(lk, cid);
}

inline int32_t lk_unlock(LEVEL_BASE::PIN_LOCK* lk)
{
    return LEVEL_BASE::ReleaseLock(lk);
}

inline void lk_init(LEVEL_BASE::PIN_LOCK* lk)
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
extern LEVEL_BASE::PIN_LOCK memory_lock;

/* Cache lock should be acquired before any access to the shared
 * caches (including enqueuing requests from lower-level caches). */
extern LEVEL_BASE::PIN_LOCK cache_lock;

/* Cycle lock is used to synchronize global state (cycle counter)
 * and not let a core advance in time without increasing cycle. */
extern LEVEL_BASE::PIN_LOCK cycle_lock;

/* Protects static pools in core_t class. */
extern LEVEL_BASE::PIN_LOCK core_pools_lock;

/* Protects static pools in core_oracle_t class. */
extern LEVEL_BASE::PIN_LOCK oracle_pools_lock;

/* Make sure printing to the console is deadlock-free */
extern LEVEL_BASE::PIN_LOCK printing_lock;

#endif // __SYNCHRONIZATION_H__
