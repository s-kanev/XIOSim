#include <cstddef>
#include <cstdint>

#include <iostream>
#include <iomanip>

const size_t NUM_ITEMS = 256 * 1024;

int32_t a[NUM_ITEMS];
int32_t b[NUM_ITEMS];

extern "C" void xiosim_roi_begin() __attribute__ ((noinline));
extern "C" void xiosim_roi_end() __attribute__ ((noinline));

void xiosim_roi_begin() { __asm__ __volatile__ ("":::"memory"); }
void xiosim_roi_end() { __asm__ __volatile__ ("":::"memory"); }

int main(int argc, char* argv[])
{
    /* Time in ROI should be ~3.5x NUM_ITEMS instructions */
    xiosim_roi_begin();

    /* Copy val to a - NUM_ITEMS unrolled instructions */
    int32_t val = 0xdecafbad;
    __asm__ __volatile__ ("cld;"
                          "rep stosl"
                          :
                          :"D"(a), "a"(val), "c"(NUM_ITEMS)
                          :"memory");

    /* Copy a to b - NUM_ITEMS unrolled instructions */
    __asm__ __volatile__ ("cld;"
                          "rep movsl"
                          :
                          :"D"(b), "S"(a), "c"(NUM_ITEMS)
                          :"memory");

    /* Compare a to b - NUM_ITEMS unrolled instructions */
    __asm__ __volatile__ ("cld;"
                          "repe cmpsl"
                          :
                          :"D"(b), "S"(a), "c"(NUM_ITEMS)
                          :"memory");

    a[NUM_ITEMS / 2] = 0;
    /* Compare a to b -- finish early - 1/2 NUM_ITEMS unrolled instructions */
    __asm__ __volatile__ ("cld;"
                          "repe cmpsl"
                          :
                          :"D"(b), "S"(a), "c"(NUM_ITEMS)
                          :"memory");
    xiosim_roi_end();

    return 0;
}
