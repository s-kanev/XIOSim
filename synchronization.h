
/* Synchronization primitives in xiosim.
 * Copyright, Svilen Kanev, 2011
 */


#ifndef __SYNCHRONIZATION_H__
#define __SYNCHRONIZATION_H__
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <utility>

inline void xio_sleep(int msecs)
{
    usleep(msecs * 1000);
}

inline void yield()
{
    pthread_yield();
}

/* Custom lock implementaion -- faster than the futeces that pin uses
 * because it stays in userspace only */

/* This is a ticket lock. It provides fairness, but is also fast.
 * Implicitly implements a queue with just two
 * 16-bit values. Once we try to grab the lock, we atomically get the
 * current value of the high-order word and increment it. We spin until
 * the low-order word becomes equal to the stored copy. Each unlock
 * increments the low-order word.
 * Credit for this goes to  Nick Piggin (https://lwn.net/Articles/267968/)
 */

class XIOSIM_LOCK {
  public:
    XIOSIM_LOCK()
    {
        __asm__ __volatile__ ("":::"memory");
        this->v = 0;
    }

    /* Enable move constructors */
    XIOSIM_LOCK(XIOSIM_LOCK &&arg) : v(std::move(arg.v)) {}
    XIOSIM_LOCK & operator= (XIOSIM_LOCK &&arg) { this->v = std::move(arg.v); return *this; }

    /* Disable copy constructors */
    XIOSIM_LOCK(const XIOSIM_LOCK &) = delete;
    XIOSIM_LOCK & operator= (const XIOSIM_LOCK &arg) = delete;

    inline void lock()
    {
        int32_t inc = 0x00010000;
        int32_t tmp = 0;
        __asm__ __volatile__ (  "lock xaddl %0, %1\n"
                                "movzwl %w0, %2\n"
                                "shrl $16, %0\n"
                                "1:\n"
                                "cmpl %0, %2\n"
                                "je 2f\n"
                                "movzwl %1, %2\n"
                                "jmp 1b\n"
                                "2:\n"
                                :"+Q" (inc), "+m" (this->v), "+r" (tmp)
                                :
                                :"memory", "cc");
    }

    inline void unlock()
    {
        __asm__ __volatile ("incw %0"
                            :"+m" (this->v)
                            :
                            :"memory", "cc");
    }

  private:
    int32_t v;

} __attribute__ ((aligned (64)));

/* These are just for compatibility and should be refactored away. */
inline void lk_lock(XIOSIM_LOCK* lk, int32_t cid)
{
    (void) cid;
    lk->lock();
}

inline int32_t lk_unlock(XIOSIM_LOCK* lk)
{
    lk->unlock();
    return 0;
}

inline void lk_init(XIOSIM_LOCK* lk)
{
}


/* Protecting shared state among cores */
/* Lock acquire order (outmost to inmost):
 - cache_lock
 - oracle_pools_lock
 */

extern XIOSIM_LOCK repeater_lock;

/* Cache lock should be acquired before any access to the shared
 * caches (including enqueuing requests from lower-level caches). */
extern XIOSIM_LOCK cache_lock;

/* Cycle lock is used to synchronize global state (cycle counter)
 * and not let a core advance in time without increasing cycle. */
extern XIOSIM_LOCK cycle_lock;

/* Protects static pools in core_oracle_t class. */
extern XIOSIM_LOCK oracle_pools_lock;

/* Make sure printing to the console is deadlock-free */
extern XIOSIM_LOCK *printing_lock;

#endif // __SYNCHRONIZATION_H__
