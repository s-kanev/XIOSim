
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

inline void lk_lock(int32_t* lk)
{
    LEVEL_BASE::GetLock(lk, 1);
}

inline int32_t lk_unlock(int32_t* lk)
{
    return LEVEL_BASE::ReleaseLock(lk);
}

inline void lk_init(int32_t* lk)
{
    LEVEL_BASE::InitLock(lk);
}

extern void spawn_new_thread(void entry_point(void*), void* arg);
