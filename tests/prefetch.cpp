#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <random>
#include <unistd.h>

extern "C" void xiosim_roi_begin() __attribute__ ((noinline));
extern "C" void xiosim_roi_end() __attribute__ ((noinline));

void xiosim_roi_begin() { __asm__ __volatile__ ("":::"memory"); }
void xiosim_roi_end() { __asm__ __volatile__ ("":::"memory"); }

struct entry {
    int32_t next_ind;
    int32_t pf_ind;
    char padding[56]; // pad to a cache line to simplify math
};
const size_t SIZE = 32 * 1024; // 2MB, so in L3
alignas(64) entry arr[SIZE];
int32_t indices[SIZE];

const size_t iterations = 10;

const int32_t PF_LOOKAHEAD = 16;

/* without -DPREFETCH: X instructions (X = 1310720, 4 * iterations * SIZE)
 * Y load lookups (Y = 327680, iterations * SIZE)
 * Y load misses
 * IPC: ~0.283
 *
 * with -DPREFETCH: 2*X  instructions (8 * iterations * SIZE)
 * 3*Y load lookups
 *      (curr->next_ind; curr->pf_ind (same line, always hits), prefetch)
 * Y load misses (same # as before, but now these are the prefetches -> PROFIT!)
 * IPC: ~1.586
 */
int chase() {
    size_t ind = 0;
    for (size_t i = 0; i < iterations * SIZE; i++) {
        entry* curr = &arr[ind];
        ind = curr->next_ind;
#ifdef PREFETCH
        __builtin_prefetch(&arr[curr->pf_ind], 0, 0);
#endif
    }
    return ind > 0; // don't optimize us away
}

int main(int argc, char* argv[]) {
    memset(arr, 0, SIZE * sizeof(entry));

    /* Generate a permutation of indices */
    for (size_t i = 0; i < SIZE; i++)
        indices[i] = i;
    // pseudo-random with fixed seed for reproducibility
    std::mt19937 rd(1);
    std::default_random_engine e1(rd());
    std::shuffle(std::begin(indices), std::end(indices), e1);

    /* Initialize array to hold a pointer chase */
    size_t ind = 0;
    for (size_t i = 0; i < SIZE; i++) {
        arr[ind].next_ind = indices[i];
        if (i < SIZE - PF_LOOKAHEAD)
            arr[ind].pf_ind = indices[i + PF_LOOKAHEAD];
        ind = indices[i];
    }

    /* Aand chase away */
    xiosim_roi_begin();
    int res = chase();
    xiosim_roi_end();

    if (res) usleep(1);
    return 0;
}
