// Repeatedly store/load and flush from a set of memory addresses.
//
// The load PCs should appear in the load_miss_pcs and store_miss_pcs stat.

#include <stdlib.h>
#include <stdio.h>

const int ITER = 10000;
const int LOG_CACHELINE_SZ = 6;
// A buffer of this many intergers must be at least twice the size of the L1 dcache.
const int BUF_SZ = 1024 * 1024;

extern "C" void xiosim_roi_begin() __attribute__ ((noinline));
extern "C" void xiosim_roi_end() __attribute__ ((noinline));

void xiosim_roi_begin() { __asm__ __volatile__ ("":::"memory"); }
void xiosim_roi_end() { __asm__ __volatile__ ("":::"memory"); }

int main(int args, char** argv) {
    int val = 0;
    int* src_buf = (int*)malloc(BUF_SZ * sizeof(int));
    // Pseuorandom with fixed seed.
    srand(0);

    xiosim_roi_begin();
    for (int i = 0; i < ITER; i++) {
        int rand_idx = (rand() % (BUF_SZ >> LOG_CACHELINE_SZ)) << LOG_CACHELINE_SZ;
        val = src_buf[rand_idx];
    }
    xiosim_roi_end();

    printf("Last value: %d\n", val);
    return 0;
}
