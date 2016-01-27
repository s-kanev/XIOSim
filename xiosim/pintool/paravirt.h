#ifndef __FEEDER_PARAVIRT__
#define __FEEDER_PARAVIRT__

/* Utilities to virtualize timing system calls.
 * So we can lie to the application and return simulated time instead of
 * elapsed on the simulation host.
 */

#include "feeder.h"

VOID InstrumentParavirt(TRACE trace, VOID* v);

/* Hook for the entrypoint of gettimeofday().
 * Called either at the syscall site or when entering the VDSO version. */
void BeforeGettimeofday(THREADID tid, ADDRINT arg1);

/* Hook for the exit point of gettimeofday(). Actually modifies the returned value.
 * Called either at the syscall return, or the ret instruction of the VDSO version. */
void AfterGettimeofday(THREADID tid, ADDRINT retval);

#endif /* __FEEDER_PARAVIRT__ */
