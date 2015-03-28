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
extern KNOB<int> KnobNumCores;

/* A list of the threads in this feeder. */
extern list<THREADID> thread_list;
extern XIOSIM_LOCK thread_list_lock;
/* Mapping from a feeder thread to a *virtual* core. We use it to let the application
 * enforce an ordering among threads. When we do something that affects all threads,
 * say, pause the simulation, we traverse thread_list in the order set by virtual_affinity.
 * Only used by HELIX for now, but we can easily hijack pthread_setaffinity. */
extern map<THREADID, int> virtual_affinity;


/* Mapping from system-wide thread pid to the Pin-local, zero-based thread id. */
extern map<pid_t, THREADID> global_to_local_tid;
extern XIOSIM_LOCK lk_tid_map;

/* Unique address space id -- the # of this feeder among all */
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
        firstIteration = false;
        lastSignalAddr = 0xdecafbad;

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

/* Pause/Resume API. The underlying abstraction is a *simulation slice*.
 * It's just an ROI that we simulate, and ignore anything in between.
 * For regular programs, that's typically one SimPoint, but we abuse it
 * for HELIX to ignore things between parallel loops.
 * The typical sequence of calls is:
 * StartSimSlice(), ResumeSimulation(), SIMULATION_HAPPENS_HERE, PauseSimulation(), EndSimSlice().
 * For HELIX, we do ResumeSimulation(), -----------------------, PauseSimulation() for every parallel loop.
*/

/* Make sure that all sim threads drain any handshake buffers that could be in
 * their respective scheduler run queues.
 * Start ignoring all produced instructions. Deallocate all cores.
 * Invariant: after this call, all sim threads are spinning in SimulatorLoop */
VOID PauseSimulation();
/* Allocate cores. Make sure threads start producing instructions.
 * Schedule threads for simulation. */
VOID ResumeSimulation(bool allocate_cores);
/* Start a new simulation slice (after waiting for all processes).
 * Add instrumentation calls and set ExecMode. */
VOID StartSimSlice(int slice_num);
/* End simulation slice (after waiting for all processes).
 * Remove all instrumetation so we can FF fast between slices. */
VOID EndSimSlice(int slice_num, int slice_length, int slice_weight_times_1000);
/* Call the current core allocator and get the # of cores we are allowed to use.
 * Some allocators use profiled scaling and serial runtimes to make their decisions.
 * Check out base_allocator.h for the API. */
int AllocateCores(std::vector<double> scaling, double serial_runtime);

/* Insert instrumentation that we didn't add so we can skip ILDJIT compilation even faster. */
VOID doLateILDJITInstrumentation();

/* Print an instruction to the dynamic pc trace. */
/* XXX: We probably need a cleaner "insert fake instruction" API */
VOID printTrace(string stype, ADDRINT pc, pid_t tid);

/* Control the "putting producer threads to sleep" optimization.
 * It helps significantly when we are crunched for cores on the simulation host
 * (e.g. simulating 16-cores on a 16-core machine). */
void disable_producers();
void enable_producers();

#endif /*__FEEDER_ZESTO__ */
