
/* Synchronization primitives provided by feeder:
 * These are wrappers to pin internal threading functions.
 * Using pthreads inside a pintool is not safe.
 * Copyright, Svilen Kanev, 2011
 */


#ifndef __SYNCHRONIZATION_H__
#define __SYNCHRONIZATION_H__

namespace LEVEL_PINCLIENT {
extern void PIN_Yield();
}

inline void yield()
{
    LEVEL_PINCLIENT::PIN_Yield();
}

#ifdef USE_PIN_LOCK
/* Wrappers around pin locks */
namespace LEVEL_BASE {
struct PIN_LOCK;
extern void GetLock(PIN_LOCK* lk, int32_t tid);
extern int32_t ReleaseLock(PIN_LOCK* lk);
extern void InitLock(PIN_LOCK* lk);
}

typedef XIOSIM_LOCK XIOSIM_LOCK;

inline void lk_lock(XIOSIM_LOCK* lk, int32_t cid)
{
    LEVEL_BASE::GetLock(lk, cid);
}

inline int32_t lk_unlock(XIOSIM_LOCK* lk)
{
    return LEVEL_BASE::ReleaseLock(lk);
}

inline void lk_init(XIOSIM_LOCK* lk)
{
    LEVEL_BASE::InitLock(lk);
}

#else
/* Custom lock implementaion -- faster than the futeces that pin uses
 * because it stays in userspace only */

typedef struct { int32_t v; } __attribute__ ((aligned (64))) XIOSIM_LOCK;

inline void lk_lock(XIOSIM_LOCK* lk, int32_t cid)
{
    __asm__ __volatile__ (  "1:\n"
                            "movl $0, %%eax\n"
                            "lock cmpxchgl %[chg], %[mem]\n"
                            "jnz 1b\n"
                            : [mem] "=m" (lk->v)
                            : [chg] "r" (cid)
                            :"%eax");
}

inline int32_t lk_unlock(XIOSIM_LOCK* lk)
{
    __asm__ __volatile__ ("":::"memory");
    lk->v = 0;
    //XXX: return value is wrong! Implement with xchg!
    return 0;
}

inline void lk_init(XIOSIM_LOCK* lk)
{
    __asm__ __volatile__ ("":::"memory");
    lk->v = 0;
}

#endif

/* Protecting shared state among cores */
/* Lock acquire order (outmost to inmost):
 - cache_lock
 - memory_lock
 - pool_lock (in oracle and core)
 */

extern XIOSIM_LOCK repeater_lock;

/* Memory lock should be acquired before functional accesses to
 * the simulated application memory. */
extern XIOSIM_LOCK memory_lock;

/* Cache lock should be acquired before any access to the shared
 * caches (including enqueuing requests from lower-level caches). */
extern XIOSIM_LOCK cache_lock;

/* Cycle lock is used to synchronize global state (cycle counter)
 * and not let a core advance in time without increasing cycle. */
extern XIOSIM_LOCK cycle_lock;

/* Protects static pools in core_t class. */
extern XIOSIM_LOCK core_pools_lock;

/* Protects static pools in core_oracle_t class. */
extern XIOSIM_LOCK oracle_pools_lock;

/* Make sure printing to the console is deadlock-free */
extern XIOSIM_LOCK printing_lock;

#endif // __SYNCHRONIZATION_H__
