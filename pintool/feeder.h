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

class handshake_container_t;
typedef queue<handshake_container_t*> handshake_queue_t;
extern KNOB<BOOL> KnobILDJIT;
extern KNOB<string> KnobFluffy;
extern map<THREADID, BOOL> ignore;
extern map<THREADID, handshake_queue_t> handshake_buffer;
extern map<THREADID, handshake_queue_t> inserted_pool;
extern PIN_LOCK simbuffer_lock;
extern PIN_LOCK printing_lock;

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
        firstWait = true;
        lastSignalID = 0xdecafbad;
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

    // Have we executed a wait on this thread
    BOOL firstWait;

    // ID of the last signal executed
    ADDRINT lastSignalID;
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
        memzero(&handshake, sizeof(P2Z_HANDSHAKE));
        handshake.real = true;
        valid = FALSE;
        isFirstInsn = true;
        isLastInsn = false;
        killThread = false;
        mem_released = true;
    }

    // Did we finish dumping context
    BOOL valid;
    // Handshake information that gets passed on to Zesto
    struct P2Z_HANDSHAKE handshake;

    // Did we finish dumping memory info
    BOOL mem_released;
    // Memory reads and writes to be passed on to Zesto
    std::map<UINT32, UINT8> mem_buffer;

    BOOL isFirstInsn;
    BOOL isLastInsn;

    BOOL killThread;
};

VOID PPointHandler(CONTROL_EVENT ev, VOID * v, CONTEXT * ctxt, VOID * ip, THREADID tid);
VOID StopSimulation(THREADID tid);
VOID SimulatorLoop(VOID* arg);
VOID ThreadFini(THREADID threadIndex, const CONTEXT *ctxt, INT32 code, VOID *v);
VOID Fini(INT32 exitCode, VOID *v);
VOID ScheduleRunQueue();

#endif /*__FEEDER_ZESTO__ */
