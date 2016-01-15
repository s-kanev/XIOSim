/*
 * Test for RDTSC timing virtualization.
 *
 * Compiled on O1, the test should measure around 100k cycles between
 * reads of the timestamp counter.
 */

#include <stdio.h>

extern "C" void xiosim_roi_begin() __attribute__ ((noinline));
extern "C" void xiosim_roi_end() __attribute__ ((noinline));
int spin() __attribute__ ((noinline));

void xiosim_roi_begin() { __asm__ __volatile__ ("":::"memory"); }
void xiosim_roi_end() { __asm__ __volatile__ ("":::"memory"); }

int spin() {
    int i;
    for (i = 0; i < 100000; i++)
        ;
    return (i < 0);
}

int main() {
    unsigned int init_lo = 0, init_hi = 0;
    unsigned int final_lo = 0, final_hi = 0;

    xiosim_roi_begin();
    __asm__ __volatile__ ("rdtsc": "=a"(init_lo), "=d"(init_hi));
    int ret = spin();
    __asm__ __volatile__ ("rdtsc": "=a"(final_lo), "=d"(final_hi));
    xiosim_roi_end();

    int hi_diff = final_hi - init_hi;
    int lo_diff = final_lo - init_lo;

    // So the spin() function doesn't get optimized away.
    printf("Some return: %d\n", ret);
    printf("High difference: %d\n", hi_diff);
    printf("Low difference: %d\n", lo_diff);

    return 0;
}
