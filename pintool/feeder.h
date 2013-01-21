#ifndef __FEEDER_ZESTO__
#define __FEEDER_ZESTO__

/*
 * Molecool: Feeder to Zesto, fed itself by ILDJIT.
 * Copyright, Vijay Reddi, 2007 -- SimpleScalar feeder prototype
              Svilen Kanev, 2011
*/

#include "pin.H"
#include "instlib.H"
#include <stack>
using namespace INSTLIB;

#include "../interface.h"
#include "../synchronization.h"

class handshake_container_t;
typedef queue<handshake_container_t*> handshake_queue_t;
extern KNOB<BOOL> KnobILDJIT;
extern KNOB<string> KnobFluffy;
extern vector<THREADID> thread_list;
extern map<UINT32, THREADID> core_threads;
extern map<THREADID, UINT32> thread_cores;


/* ========================================================================== */
// Thread-private state that we need to preserve between different instrumentation calls
class thread_state_t
{
  class per_loop_state_t {
    public:
      per_loop_state_t() {
	unmatchedWaits = 0;
      }
      
      INT32 unmatchedWaits;
  };
  
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
        firstIteration = false;
        lastSignalAddr = 0xdecafbad;

        memset(pc_queue, 0, PC_QUEUE_SIZE*sizeof(INT32));
        pc_queue_head = PC_QUEUE_SIZE-1;
        pc_queue_valid = false;

        ignore = true;
        ignore_all = true;

        is_running = true;
        num_inst = 0;

	sleep_producer = false;

        lk_init(&lock);
    }
    
    VOID push_loop_state()
    {
      per_loop_stack.push(per_loop_state_t());
      loop_state = &(per_loop_stack.top());
    }        
    
    VOID pop_loop_state()
    {
      per_loop_stack.pop();
      if(per_loop_stack.size()) {
	loop_state = &(per_loop_stack.top());
      }
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

    // How many instructions have been produced
    UINT64 num_inst;

    // Have we executed a wait on this thread
    BOOL firstIteration;

    // Address of the last signal executed
    ADDRINT lastSignalAddr;

    // Per Loop State
    per_loop_state_t* loop_state;


    // Handling a buffer of the last PC_QUEUE_SIZE instruction pointers
    ADDRINT get_queued_pc(INT32 index) {
        return pc_queue[(pc_queue_head + index) & (PC_QUEUE_SIZE - 1)];
    }
    VOID queue_pc(ADDRINT pc) {
        pc_queue_head = (pc_queue_head - 1) & (PC_QUEUE_SIZE - 1);
        pc_queue[pc_queue_head] = pc;
        pc_queue_valid = true;
    }
    BOOL pc_queue_valid;

    XIOSIM_LOCK lock;
    // XXX: SHARED -- lock protects those
    // Signal to the simulator thread to die
    BOOL is_running;
    // Set by simulator thread once it dies
    BOOL sim_stopped;
    // Is thread not instrumenting instructions ?
    BOOL ignore;
    // Similar effect to above, but produced differently for sequential code
    BOOL ignore_all;
    // Stores the ID of the wait between before and afterWait. -1 outside.
    INT32 lastWaitID;
    // flag on whether to sleep the producer buffer
    BOOL sleep_producer;

    // XXX: END SHARED

private:
    std::stack<per_loop_state_t> per_loop_stack;
    // XXX: power of 2
    static const INT32 PC_QUEUE_SIZE = 4;
    // Latest several pc-s instrumented
    ADDRINT pc_queue[PC_QUEUE_SIZE];
    INT32 pc_queue_head;
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

struct handshake_flags_t
{
  // Did we finish dumping context
  BOOL valid;

  BOOL isFirstInsn;
  BOOL isLastInsn;
  BOOL killThread;
};

class handshake_container_t
{
  public:
    handshake_container_t() {
        Clear();
    }

    void Clear() {
        memzero(&handshake, sizeof(P2Z_HANDSHAKE));
        handshake.real = true;
        flags.valid = false;
        flags.isFirstInsn = false;
        flags.isLastInsn = false;
        flags.killThread = false;

        mem_buffer.clear();
    }

    void CopyTo(handshake_container_t* dest) const {
        dest->flags = this->flags;
        memcpy(&dest->handshake, &this->handshake, sizeof(P2Z_HANDSHAKE));
        dest->mem_buffer = this->mem_buffer;
    }

    // Handshake information that gets passed on to Zesto
    struct P2Z_HANDSHAKE handshake;

    struct handshake_flags_t flags;

    // Memory reads and writes to be passed on to Zesto
    std::map<UINT32, UINT8> mem_buffer;
};

VOID PPointHandler(CONTROL_EVENT ev, VOID * v, CONTEXT * ctxt, VOID * ip, THREADID tid);
VOID StopSimulation(THREADID tid);
VOID SimulatorLoop(VOID* arg);
VOID ThreadFini(THREADID threadIndex, const CONTEXT *ctxt, INT32 code, VOID *v);
VOID Fini(INT32 exitCode, VOID *v);
VOID ScheduleRunQueue();
VOID PauseSimulation(THREADID tid);
VOID ResumeSimulation(THREADID tid);

VOID amd_hack();
VOID doLateILDJITInstrumentation();

VOID disable_consumers();
VOID enable_consumers();
VOID disable_producers();
VOID enable_producers();

VOID flushOneToHandshakeBuffer(THREADID tid);
VOID flushLookahead(THREADID tid, int numToIgnore);

VOID printTrace(string stype, ADDRINT pc, THREADID tid);

#endif /*__FEEDER_ZESTO__ */
