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
    thread_state_t(THREADID instrument_tid) {
        memzero(&fpstate_buf, sizeof(FPSTATE));

        last_syscall_number = last_syscall_arg1 = 0;
        last_syscall_arg2 = last_syscall_arg3 = 0;
        bos = -1;
        slice_num = 0;
        slice_length = 0;
        slice_weight_times_1000 = 0;
        coreID = -1;
    }

    // Buffer to store the fpstate that the simulator may corrupt
    FPSTATE fpstate_buf;

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

    // Which simulated core this thread runs on
    ADDRINT coreID;
};
thread_state_t* get_tls(ADDRINT tid);

/* ========================================================================== */
/* Execution mode allows easy querying of exactly what the pin tool is doing at
 * a given time, and also helps ensuring that certain parts of the code are run
 * in only certain modes. */
enum EXECUTION_MODE
{
    EXECUTION_MODE_FASTFORWARD,
    EXECUTION_MODE_SIMULATE,
    EXECUTION_MODE_INVALID
};
extern EXECUTION_MODE ExecMode;

class handshake_container_t
{
  public:
    handshake_container_t() {
        memzero(&regstate, sizeof(regs_t));
        memzero(&handshake, sizeof(P2Z_HANDSHAKE));
        valid = FALSE;
        isFirstInsn = TRUE;
        isLastInsn = FALSE;
        killThread = FALSE;
    }

    // Did we finish dumping context
    BOOL valid;

    // Register state as seen by Zesto
    regs_t regstate;

    // Handshake information that gets passed on to Zesto
    struct P2Z_HANDSHAKE handshake;

    BOOL isFirstInsn;
    BOOL isLastInsn;

    // Time to let simulator thread exit?
    BOOL killThread;
};

VOID PPointHandler(CONTROL_EVENT ev, VOID * v, CONTEXT * ctxt, VOID * ip, THREADID tid);
VOID SimulatorLoop(VOID* arg);
VOID Fini(INT32 exitCode, VOID *v);
VOID ScheduleRunQueue(VOID);

#endif /*__FEEDER_ZESTO__ */
