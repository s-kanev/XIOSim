extern "C" void xiosim_roi_begin() __attribute__ ((noinline));
extern "C" void xiosim_roi_end() __attribute__ ((noinline));

void xiosim_roi_begin() { __asm__ __volatile__ ("":::"memory"); }
void xiosim_roi_end() { __asm__ __volatile__ ("":::"memory"); }

int main(int argc, char* argv[]) {
    const int ITER = 1000000;

    xiosim_roi_begin();

    /* The loop has 7 Mops. On NHM, it is forced to at least 2 cycles / iteration
     * (only one jump unit). If we don't fuse, we still have 5 more ops to do
     * during the 2 cycles, but only 2 IEUs, so we're forced to 3 cycles / iteration.
     * If we fuse, there's 5 fused uops. The cmpjmps use the JEU (so, 2 cycles),
     * but there's enough IEUs now to finish 3 adds in 2 cycles / iteration. */
    __asm__ __volatile__ (
            "1:movl $0, %%eax;"
            "movl $0, %%ebx;"
            "movl $0, %%ecx;"
            "loop: cmpl %%ebx, %%ecx;"
            "jne 1b;"
            "addl $1, %%ebx;"
            "addl $1, %%ecx;"
            "addl $1, %%eax;"
            "cmpl %0, %%eax;"
            "jne loop;"
            :
            :"n"(ITER)
            :"%eax","%ebx","%ecx");

    xiosim_roi_end();

    return 0;
}
