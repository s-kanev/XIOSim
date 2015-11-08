#include <cstdio>
#include <cstdlib>

extern "C" void xiosim_roi_begin() __attribute__ ((noinline));
extern "C" void xiosim_roi_end() __attribute__ ((noinline));

void xiosim_roi_begin() { __asm__ __volatile__ ("":::"memory"); }
void xiosim_roi_end() { __asm__ __volatile__ ("":::"memory"); }

int main(int argc, char* argv[]) {

    const int32_t NUM_TERS = 10000;
    xiosim_roi_begin();

    /* If we don't ignore the nop, we'll execute 40K instructions.
     * If we do -> 30K.
     * In the currently compiled version, invoke with -ignore 0x40108a */
    __asm__ __volatile ("loop: nop;"
                        "subl $1, %%eax;"
                        "cmpl $0, %%eax;"
                        "jns loop;"
                        :
                        : "a"(NUM_TERS)
                        : "memory");

    xiosim_roi_end();
    return 0;
}
