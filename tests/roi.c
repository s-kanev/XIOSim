// g++ -O0 -m32 -mtune=atom -static -o roi roi.c
// TODO(skanev): add a makefile for ubenchmarks

#include <cstdio>
#include <cstdlib>

int fib(int n)
{
    if(n <= 1)
        return 1;
    else
        return fib(n-1) + fib(n-2);
}

extern "C" void xiosim_roi_begin() __attribute__ ((noinline));
extern "C" void xiosim_roi_end() __attribute__ ((noinline));

void xiosim_roi_begin() { __asm__ __volatile__ ("":::"memory"); }
void xiosim_roi_end() { __asm__ __volatile__ ("":::"memory"); }

int main(int argc, char* argv[])
{
    const int lim = 19;
    fprintf(stderr, "fib(%d) = %d\n", lim, fib(lim));
    // Time in ROI should be ~217 K instructions
    xiosim_roi_begin();
    fprintf(stderr, "fib(%d) = %d\n", lim, fib(lim));
    xiosim_roi_end();
    fprintf(stderr, "fib(%d) = %d\n", lim, fib(lim));
    return 0;
}
