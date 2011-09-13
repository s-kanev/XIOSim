#ifndef __FEEDER_ZESTO__
#define __FEEDER_ZESTO__

/*                      
 * Molecool: Feeder to Zesto, fed itself by ILDJIT.
 * Copyright, Vijay Reddi, 2007 -- SimpleScalar feeder prototype 
              Svilen Kanev, 2011
*/
#include "pin.H"
#include "instlib.H"
using namespace INSTLIB;

#include "../interface.h" 

extern KNOB<BOOL> KnobILDJIT;
extern KNOB<string> KnobFluffy;

/* ========================================================================== */
// Thread-private state that we need to preserve between different instrumentation calls
class thread_state_t
{
  public:
    thread_state_t() {
        memzero(&fpstate_buf, sizeof(FPSTATE));
        memzero(&regstate, sizeof(regs_t));
        memzero(&handshake, sizeof(P2Z_HANDSHAKE));

        last_syscall_number = last_syscall_arg1 = 0;
        last_syscall_arg2 = last_syscall_arg3 = 0;
        bos = -1;
        slice_num = 0;
        slice_length = 0;
        slice_weight_times_1000 = 0;
    }

    // Buffer to store the fpstate that the simulator may corrupt
    FPSTATE fpstate_buf;

    // Register state  as seen by Zesto
    regs_t regstate;

    // Handshake information that gets passed on to Zesto
    struct P2Z_HANDSHAKE handshake;

    // Used by syscall capture code
    ADDRINT last_syscall_number;
    ADDRINT last_syscall_arg1;
    ADDRINT last_syscall_arg2;
    ADDRINT last_syscall_arg3;

    // Bottom-of-stack pointer used for shadow page table
    ADDRINT bos;

    // Pinpoints-related
    ADDRINT slice_num;
    ADDRINT slice_length;
    ADDRINT slice_weight_times_1000;
};
thread_state_t* get_tls(ADDRINT tid);

VOID PPointHandler(CONTROL_EVENT ev, VOID * v, CONTEXT * ctxt, VOID * ip, THREADID tid);

#endif /*__FEEDER_ZESTO__ */
