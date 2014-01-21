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
#include "../memory.h"

class handshake_container_t;

extern KNOB<BOOL> KnobILDJIT;
extern KNOB<string> KnobFluffy;
extern KNOB<int> KnobNumCores;
extern list<THREADID> thread_list;
extern XIOSIM_LOCK thread_list_lock;

extern map<pid_t, THREADID> global_to_local_tid;
extern XIOSIM_LOCK lk_tid_map;

extern INT32 host_cpus;
extern map<THREADID, tick_t> lastConsumerApply;

extern int asid;

#define ATOMIC_ITERATE(_list, _it, _lock) \
    for (lk_lock(&(_lock), 1), (_it) = (_list).begin(), lk_unlock(&(_lock)); \
         [&]{lk_lock(&(_lock), 1); bool res = (_it) != (_list).end(); lk_unlock(&(_lock)); return res;}();  \
         lk_lock(&(_lock), 1), (_it)++, lk_unlock(&(_lock)))


/* ========================================================================== */
/* Thread-local state for instrument threads that we need to preserve between
 * different instrumentation calls */
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
        memset(&fpstate_buf, 0, sizeof(FPSTATE));

        last_syscall_number = last_syscall_arg1 = 0;
        last_syscall_arg2 = last_syscall_arg3 = 0;
        bos = -1;
        firstIteration = false;
        lastSignalAddr = 0xdecafbad;

        memset(pc_queue, 0, PC_QUEUE_SIZE*sizeof(INT32));
        pc_queue_head = PC_QUEUE_SIZE-1;
        pc_queue_valid = false;

        ignore = true;
        ignore_all = true;
        firstInstruction = true;

        num_inst = 0;
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

    // Return PC for routines that we ignore (e.g. ILDJIT callbacks)
    ADDRINT retPC;

    // How many instructions have been produced
    UINT64 num_inst;

    // Have we executed a wait on this thread
    BOOL firstIteration;

    // Address of the last signal executed
    ADDRINT lastSignalAddr;

    // Global tid for this thread
    pid_t tid;

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
    // Is thread not instrumenting instructions ?
    BOOL ignore;
    // Similar effect to above, but produced differently for sequential code
    BOOL ignore_all;
    // Stores the ID of the wait between before and afterWait. -1 outside.
    INT32 lastWaitID;

    BOOL firstInstruction;
    // XXX: END SHARED

private:
    std::stack<per_loop_state_t> per_loop_stack;
    // XXX: power of 2
    static const INT32 PC_QUEUE_SIZE = 4;
    // Latest several pc-s instrumented
    ADDRINT pc_queue[PC_QUEUE_SIZE];
    INT32 pc_queue_head;
};
thread_state_t* get_tls(THREADID tid);

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


VOID PPointHandler(CONTROL_EVENT ev, VOID * v, CONTEXT * ctxt, VOID * ip, THREADID tid);
VOID PauseSimulation();
VOID StopSimulation(BOOL kill_sim_threads);
VOID SimulatorLoop(VOID* arg);
VOID ThreadFini(THREADID threadIndex, const CONTEXT *ctxt, INT32 code, VOID *v);
VOID Fini(INT32 exitCode, VOID *v);

VOID amd_hack();
VOID doLateILDJITInstrumentation();

VOID printTrace(string stype, ADDRINT pc, pid_t tid);

#endif /*__FEEDER_ZESTO__ */
