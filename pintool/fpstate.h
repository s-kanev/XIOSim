#ifndef __FPSATTE_H__
#define __FPSTATE_H__

// Saves and restores fp state (x87, MMX, SSE) to a static buffer

/* XXX: This will blatantly fail if the simulator is compiled 
        with AVX support and the user application uses AVX.
        Until we have a SNB to test, no YMM saving with XSAVE */

__inline__ void fxsave(char *buf)
{
    __asm__ __volatile__ (
            "fxsave %0"
            :"=m"(buf[0]));
}

__inline__ void fxrstor(char *buf)
{
    __asm__ __volatile (
        "fxrstor %0 \n fwait"
        ::"m" (buf[0]));

}

#endif /* __FPSTATE_H__ */
