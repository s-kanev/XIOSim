#ifndef __SPECULATION_H__
#define __SPECULATION_H__

#include "pin.H"

/* This speculative process is done producing handshakes. Exits.
 * This can (and should) be called from various places in the feeder
 * (e.g. syscalls), where continuing on the speculative path would cause
 * irreversible global state changes.
 */
void FinishSpeculation(thread_state_t* tstate);

/* Add speculation-specific instrumentation. */
VOID InstrumentSpeculation(INS ins, VOID* v);

/* Is this a throwaway process on a speculative execution path. */
extern bool speculation_mode;

#endif /* __SPECULATION_H__ */
