// g++ -O3 -m32 -mtune=atom -static -o step step.c
// TODO(skanev): add a makefile for ubenchmarks

// Step in IPC from high to low

#define COMPUTE_ITER 12000
#define LOW_ITER 60000
#define RATIO 20

#include <cstdlib>

int main(int argc, char** argv)
{
    int arr[LOW_ITER / RATIO];
    int sum = 0;

    // Init array to throw off predictor
    // A.cfg IPC = 0.92
    for (int i=0; i < LOW_ITER / RATIO; i++)
        arr[i] = rand() % 10;

    // Do some compute
    // A.cfg IPC = 0.70
    for (int j=0; j < COMPUTE_ITER; j++) {
        sum += j;
        if (j % 5 == 0)
            sum *= 13;
    }

    // This should throw off the branch predictor,
    // leading to lowish IPC.
    // A.cfg IPC = 0.56
    for (int k=0; k < LOW_ITER; k++)
        if (arr[k % (LOW_ITER / RATIO) < 5])
            sum -= 1;
        else {
            __asm__("nop; nop; nop;");
            __asm__("nop; nop; nop;");
            __asm__("nop; nop; nop;");
        }

    return (sum <= 0);
}
