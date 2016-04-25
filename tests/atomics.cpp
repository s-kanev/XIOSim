#include <sys/time.h>
#include <cinttypes>
#include <cstdio>

extern "C" void xiosim_roi_begin() __attribute__((noinline));
extern "C" void xiosim_roi_end() __attribute__((noinline));

void xiosim_roi_begin() { __asm__ __volatile__("" :: : "memory"); }
void xiosim_roi_end() { __asm__ __volatile__("" :: : "memory"); }

inline uint64_t compare_and_swap(volatile uint64_t* ptr, uint64_t old_val, uint64_t new_val) {
    uint64_t prev;
    __asm__ __volatile__("cmpxchgq %1, %2"
                         : "=a"(prev)
                         : "q"(new_val), "m"(*ptr), "0"(old_val)
                         : "memory");
    return prev;
}

inline uint64_t compare_and_swap_atomic(volatile uint64_t* ptr, uint64_t old_val,
                                        uint64_t new_val) {
    uint64_t prev;
    __asm__ __volatile__("lock; cmpxchgq %1, %2"
                         : "=a"(prev)
                         : "q"(new_val), "m"(*ptr), "0"(old_val)
                         : "memory");
    return prev;
}

inline uint64_t compare_and_swap_rr(uint64_t dst, uint64_t old_val, uint64_t new_val) {
    uint64_t prev;
    __asm__ __volatile__("cmpxchgq %1, %2"
                         : "=a"(prev)
                         : "q"(new_val), "q"(dst), "0"(old_val)
                         : "memory");
    return prev;
}

inline void exchange(volatile uint64_t* ptr, uint64_t new_val) {
    __asm__ __volatile__("xchgq %0, %1"
                         :
                         : "q"(new_val), "m"(*ptr)
                         : "memory");
}

typedef void (*routine)(void);
const size_t ITERATIONS = 5000000;

void measure(routine rtn) {
    struct timeval start, end;

    gettimeofday(&start, NULL);
    rtn();
    gettimeofday(&end, NULL);

    double usec = (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_usec - start.tv_usec);
    usec /= ITERATIONS;
    printf("Time: %2.5f us\n", usec);
}

struct alignas(64) cacheline {
    uint64_t val;
};

const size_t LINES = 1024;
cacheline loc[LINES] = { 0xdeadbeef };

/* Bang on a single cache line that we have exclusive. No locks.
 * Measured 8 cycles on HSW. */
void cas_throughput() {
    for (size_t i = 0; i < ITERATIONS; i++) {
        compare_and_swap(&loc[0].val, 0, 7);
    }
}

/* Bang on a single cache line that we have exclusive.
 * Measured 19 cycles on HSW. */
void cas_atomic_throughput() {
    for (size_t i = 0; i < ITERATIONS; i++) {
        compare_and_swap_atomic(&loc[0].val, 0, 7);
    }
}

/* Bang on a single cache line that we have exclusive.
 * Measured 19 cycles on HSW. */
void xchg_throughput() {
    for (size_t i = 0; i < ITERATIONS; i++) {
        exchange(&loc[0].val, 7);
    }
}

int main() {

    xiosim_roi_begin();
    measure(xchg_throughput);
    xiosim_roi_end();

    return 0;
}
