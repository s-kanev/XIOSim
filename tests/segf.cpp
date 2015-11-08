// Trigger a segfault on a non-taken path. Should cleanly kill speculative process.

#define ITER 2000

#include <cstdlib>

int main(int argc, char** argv)
{
    int sum = 0;
    for (int i=0; i < ITER; i++) {
        if (i >= 0) {
            sum += i;
            sum *= 5;
        }
        else {
            // Test loads from addr 0
            int kaboom = *(int*)NULL;
            (void) kaboom;
        }
    }

    for (int i=0; i < ITER; i++) {
        if (i >= 0) {
            sum += i;
            sum *= 5;
        }
        else {
            // Test indirect jumps to addr 0
            __asm__ ("xor %%ecx, %%ecx\n"
                     "jmp *(%%ecx)" :::"%ecx");
        }
    }

    return 0;
}
