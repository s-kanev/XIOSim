// g++ -O0 -m32 -mtune=atom -static -o repl repl.c
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

extern "C" int fib_repl(int n) { return fib(n); }

int main(int argc, char* argv[])
{
    // If we properly ignored the longer fib_repl call, we should take ~450K instructions.
    const int lim = 19;
    fprintf(stderr, "fib(%d) = %d\n", lim, fib(lim));
    fprintf(stderr, "fib(%d) = %d\n", lim, fib_repl(lim+2));
    fprintf(stderr, "fib(%d) = %d\n", lim, fib(lim));
    return 0;
}
