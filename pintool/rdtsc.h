#ifndef __RDTSC_H__
#define __RDTSC_H__


// Helper function: Execute an rdtsc from tool to get current tsc (works fine because tsc is shared for the machine)
__inline__ uint64_t rdtsc() 
{

    // serialize the machine's pipeline
    __asm__ __volatile__ (
            "xorl %%eax,%%eax \n cpuid" 
            ::: "%rax", "%rbx", "%rcx", "%rdx");

    // "lo" holds the lower 64 bits of rdtsc result, which is in %eax ("=a")
    //  - inline syntax =a tells the compiler to read the value from register EAX
    //
    // "hi" holds the upper 64 bits of rdtsc, which is in %edx ("=d")
    //  - inline syntax =d tells the compiler to read the value of register EDX
    //
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));

    // create a 64bit result
    return (uint64_t) hi << 32 | lo;
}

#endif /* __RDTSC_H__ */
