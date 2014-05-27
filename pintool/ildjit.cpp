/*
 * ILDJIT-specific functions for zesto feeder
 * Copyright, Svilen Kanev, 2012
 */
#include <stdlib.h>
#include <unistd.h>
#include <map>
#include <queue>
#include <stack>

#include "boost_interprocess.h"

#include "feeder.h"

#include "../interface.h"
#include "multiprocess_shared.h"
#include "ipc_queues.h"


#include "../buffer.h"
#include "BufferManagerProducer.h"
#include "ignore_ins.h"

#include "scheduler.h"
#include "ildjit.h"
#include "fluffy.h"
#include "utils.h"
#include "parse_speedup.h"

#include "../zesto-core.h"

// True if ILDJIT has finished compilation and is executing user code
BOOL ILDJIT_executionStarted = false;

// True if a non-compiler thread is being created
BOOL ILDJIT_executorCreation = false;

// The thread id for the main execution thread
THREADID ILDJIT_executorTID = -1;

XIOSIM_LOCK ildjit_lock;

/* A load instruction with immediate address */
const UINT8 wait_template_1_ld[] = {0xa1, 0xad, 0xfb, 0xca, 0xde};
/* A store instruction with immediate address and data */
const UINT8 wait_template_1_st[] = {0xc7, 0x05, 0xad, 0xfb, 0xca, 0xde, 0x00, 0x00, 0x00, 0x00 };

/* A MFENCE instruction */
const UINT8 wait_template_2_mfence[] = {0x0f, 0xae, 0xf0};
/* A LFENCE instruction */
const UINT8 wait_template_2_lfence[] = {0x0f, 0xae, 0xe8};


static const UINT8 * wait_template_1;
static size_t wait_template_1_size;
static size_t wait_template_1_addr_offset;

static const UINT8 * wait_template_2;
static size_t wait_template_2_size;

KNOB<BOOL> KnobWaitsAsLoads(KNOB_MODE_WRITEONCE,    "pintool",
        "waits_as_loads", "false", "Wait instructions seen as loads");

/* A store instruction with immediate address and data */
const UINT8 signal_template[] = {0xc7, 0x05, 0xad, 0xfb, 0xca, 0xde, 0x00, 0x00, 0x00, 0x00 };
/* A MFENCE instrction */
const UINT8 mfence_template[] = {0x0f, 0xae, 0xf0};
/* An INT 80 instruction */
const UINT8 syscall_template[] = {0xcd, 0x80};

static map<string, UINT32> invocation_counts;

KNOB<BOOL> KnobDisableWaitSignal(KNOB_MODE_WRITEONCE,     "pintool",
        "disable_wait_signal", "false", "Don't insert any waits or signals into the pipeline");
KNOB<BOOL> KnobCoupledWaits(KNOB_MODE_WRITEONCE,     "pintool",
        "coupled_waits", "false", "Wait semantics: coupled == one iteration unblocking the next, uncoupled == all iterations unblocking one");
KNOB<BOOL> KnobInsertLightWaits(KNOB_MODE_WRITEONCE,     "pintool",
        "insert_light_waits", "false", "Insert light waits in the processor pipeline");
KNOB<string> KnobPredictedSpeedupFile(KNOB_MODE_WRITEONCE,   "pintool",
        "speedup_file", "", "File containing speedup prediction for core allocation");
extern KNOB<BOOL> KnobWarmLLC;

static string warm_loop = "";
static UINT32 warm_loop_invocation = -1;
static UINT32 warm_loop_iteration = -1;

static string start_loop = "";
static UINT32 start_loop_invocation = -1;
static UINT32 start_loop_iteration = -1;

static string end_loop = "";
static UINT32 end_loop_invocation = -1;
static UINT32 end_loop_iteration = -1;

static BOOL first_invocation = true;

static BOOL reached_warm_invocation = false;
static BOOL reached_start_invocation = false;
static BOOL reached_end_invocation = false;

static BOOL reached_start_iteration = false;
static BOOL reached_end_iteration = false;
static BOOL simulating_parallel_loop = false;

UINT32 getSignalAddress(ADDRINT ssID);
static void initializePerThreadLoopState(THREADID tid);

static bool loopMatches(string loop, UINT32 invocationNum, UINT32 iterationNum);
static void readLoop(ifstream& fin, string* name, UINT32* invocation, UINT32* iteration);

static VOID shutdownSimulation(THREADID tid);
extern VOID doLateILDJITInstrumentation();

stack<loop_state_t> loop_states;
loop_state_t* loop_state = NULL;

bool disable_wait_signal;
bool coupled_waits;
bool insert_light_waits;

// The number of allocated cores per loop, read by ildjit
UINT32* allocated_cores;

/* Threads associated with this feeder, sorted by declared affinity,
 * so that they form the nice ring HELIX relies on. */
vector<THREADID> affine_threads;
map<THREADID, int> virtual_affinity;
XIOSIM_LOCK lk_affine_threads;

static bool ignoreSignalZero;

extern map<ADDRINT, string> pc_diss;

/* ========================================================================== */
VOID MOLECOOL_Init()
{
    lk_init(&ildjit_lock);
    lk_init(&lk_affine_threads);

    FLUFFY_Init();

    ifstream warm_loop_file, start_loop_file, end_loop_file;
    warm_loop_file.open("phase_warm_loop", ifstream::in);
    start_loop_file.open("phase_start_loop", ifstream::in);
    end_loop_file.open("phase_end_loop", ifstream::in);

    if (start_loop_file.fail()) {
        cerr << "Couldn't open loop id files: phase_start_loop" << endl;
        PIN_ExitProcess(1);
    }
    if (end_loop_file.fail()) {
        cerr << "Couldn't open loop id files: phase_end_loop" << endl;
        PIN_ExitProcess(1);
    }
    
    readLoop(start_loop_file, &start_loop, &start_loop_invocation, &start_loop_iteration);
    readLoop(end_loop_file, &end_loop, &end_loop_invocation, &end_loop_iteration);

    if(KnobWarmLLC.Value()) {
        if (warm_loop_file.fail()) {
            cerr << "Couldn't open loop id files: phase_warm_loop" << endl;
            cerr << "Warming from start" << endl;
            reached_warm_invocation = true;
        }
        else {
            readLoop(warm_loop_file, &warm_loop, &warm_loop_invocation, &warm_loop_iteration);
        }
    }
    
    disable_wait_signal = KnobDisableWaitSignal.Value();
    coupled_waits = KnobCoupledWaits.Value();
    insert_light_waits = KnobInsertLightWaits.Value();

    last_time = time(NULL);

    cerr << warm_loop << endl;
    cerr << warm_loop_invocation << endl;
    cerr << warm_loop_iteration << endl << endl;
    cerr << start_loop << endl;
    cerr << start_loop_invocation << endl;
    cerr << start_loop_iteration << endl << endl;
    cerr << end_loop << endl;
    cerr << end_loop_invocation << endl;
    cerr << end_loop_iteration << endl << endl;

    if (KnobWaitsAsLoads.Value()) {
        wait_template_1 = wait_template_1_ld;
        wait_template_1_size = sizeof(wait_template_1_ld);
        wait_template_1_addr_offset = 1; // 1 opcode byte

        wait_template_2 = wait_template_2_lfence;
        wait_template_2_size = sizeof(wait_template_2_lfence);
        *waits_as_loads = true;
    }
    else {
        wait_template_1 = wait_template_1_st;
        wait_template_1_size = sizeof(wait_template_1_st);
        wait_template_1_addr_offset = 2; // 1 opcode byte, 1 ModRM byte

        wait_template_2 = wait_template_2_mfence;
        wait_template_2_size = sizeof(wait_template_2_mfence);
        *waits_as_loads = false;
    }

    *ss_curr = 100000;
    *ss_prev = 100000;

    if (!KnobPredictedSpeedupFile.Value().empty())
        LoadHelixSpeedupModelData(KnobPredictedSpeedupFile.Value().c_str());
}

/* ========================================================================== */
BOOL ILDJIT_IsExecuting()
{
    bool res;
    lk_lock(&ildjit_lock, 1);
    res = ILDJIT_executionStarted;
    lk_unlock(&ildjit_lock);
    return res;
}

/* ========================================================================== */
BOOL ILDJIT_IsDoneFastForwarding()
{
    return reached_start_invocation;
}

/* ========================================================================== */
BOOL ILDJIT_GetExecutingTID()
{
    return ILDJIT_executorTID;
}

/* ========================================================================== */
BOOL ILDJIT_IsCreatingExecutor()
{
    bool res;
    lk_lock(&ildjit_lock, 1);
    res = ILDJIT_executorCreation;
    lk_unlock(&ildjit_lock);
    return res;
}

/* ========================================================================== */
VOID ILDJIT_startSimulation(THREADID tid, ADDRINT ip)
{
    CODECACHE_FlushCache();

    lk_lock(&ildjit_lock, 1);

    /* We are stopping thread creation here, beacuse we can capture the real
     * thread creation in Pin only on starting the thread (first insn), which
     * happens after the actual syscalls.
     * XXX: This way we can capture the creation of some compiler threads,
     * but this is generally fine, since they won't get executed */
    ILDJIT_executorCreation = false;

    ILDJIT_executionStarted = true;

    ILDJIT_executorTID = tid;

//#ifdef ZESTO_PIN_DBG
    cerr << "Starting execution, TID: " << tid << endl;
//#endif

    lk_unlock(&ildjit_lock);

    if(reached_warm_invocation) {
        cerr << "Do Early!" << endl;
        doLateILDJITInstrumentation();
        cerr << "Done Early!" << endl;
    }
}

/* ========================================================================== */
VOID ILDJIT_setupInterface(ADDRINT coreCount)
{
    allocated_cores = reinterpret_cast<UINT32*>(coreCount);
}

/* ========================================================================== */

VOID ILDJIT_endSimulation(THREADID tid, ADDRINT ip)
{
    // This should cover the helix case
    if (end_loop.size() != 0)
        return;

    // If we reach this, we're done with all parallel loops, just exit
//#ifdef ZESTO_PIN_DBG
    cerr << "Stopping simulation, TID: " << tid << endl;
//#endif

    if (KnobFluffy.Value().empty())
        PauseSimulation();
}

/* ========================================================================== */
VOID ILDJIT_ExecutorCreate(THREADID tid)
{
    lk_lock(&ildjit_lock, 1);

    ILDJIT_executorCreation = true;

//#ifdef ZESTO_PIN_DBG
    cerr << "Starting creation, TID: " << tid << endl;
//#endif

    lk_unlock(&ildjit_lock);
}

/* ========================================================================== */
VOID ILDJIT_ExecutorCreateEnd(THREADID tid)
{
    //Dummy, actual work now done in ILDJIT_ThreadStarting
}

/* =========================================================================== */

VOID ILDJIT_startLoop(THREADID tid, ADDRINT ip, ADDRINT loop)
{
    // This is for when there are serial loops within an executing parallel loop.
    if (simulating_parallel_loop) {
        thread_state_t* tstate = get_tls(tid);
        lk_lock(&tstate->lock, tid+1);
        tstate->ignore = true;
        lk_unlock(&tstate->lock);
    }

    string loop_string = (string)(char*)loop;

    // Increment invocation counter for this loop
    if(invocation_counts.count(loop_string) == 0) {
        invocation_counts[loop_string] = 0;
    }
    else {
        invocation_counts[loop_string]++;
    }

    if(KnobWarmLLC.Value()) {
        if((!reached_warm_invocation) && (warm_loop == loop_string) && (invocation_counts[loop_string] == warm_loop_invocation)) {
            assert(invocation_counts[loop_string] == warm_loop_invocation);
            cerr << "Called warmLoop() for the warm invocation!:" << loop_string << endl;
            reached_warm_invocation = true;
            cerr << "Detected that we need to warm!:" << loop_string << endl;
            cerr << "FastWarm runtime:";
            printElapsedTime();
            cerr << "Do late!" << endl;
            doLateILDJITInstrumentation();
            cerr << "Done late!" << endl;      
        }
    }

    if((!reached_start_invocation) && (start_loop == loop_string) && (invocation_counts[loop_string] == start_loop_invocation)) {
        assert(invocation_counts[loop_string] == start_loop_invocation);
        cerr << "Called startLoop() for the start invocation!:" << loop_string << endl;
        reached_start_invocation = true;
        if(start_loop_iteration == (UINT32)-1) {
            cerr << "Detected that we need to start the next parallel loop!:" << loop_string << endl;
            reached_start_iteration = true;
        }
    }

    if((!reached_end_invocation) && (end_loop == loop_string) && (invocation_counts[loop_string] == end_loop_invocation)) {
        assert(invocation_counts[loop_string] == end_loop_invocation);
        cerr << "Called startLoop() for the end invocation!:" << (CHAR*)loop << endl;
        reached_end_invocation = true;
        if(end_loop_iteration == (UINT32)-1) {
            cerr << "Detected that we need to end the next parallel loop!:" << loop_string << endl;
            reached_end_iteration = true;
        }
    }
}

/* ========================================================================== */
VOID ILDJIT_startLoop_after(THREADID tid, ADDRINT ip)
{
    // This is for when there are serial loops within an executing parallel loop.
    if (simulating_parallel_loop) {
        thread_state_t* tstate = get_tls(tid);
        lk_lock(&tstate->lock, tid+1);
        tstate->ignore = false;
        lk_unlock(&tstate->lock);
    }
}

/* ========================================================================== */
VOID ILDJIT_startInitParallelLoop(ADDRINT loop)
{
    if (!reached_start_invocation)
        return;

    vector<double> scaling;
    double serial_runtime = 0;

    if (!KnobPredictedSpeedupFile.Value().empty())
    {
        string loop_name((const char*)loop);
        scaling = GetHelixLoopScaling(loop_name);
        loop_data* data = GetHelixFullLoopData(loop_name);
        serial_runtime = data->serial_runtime;
    }

    ipc_message_t msg;
    msg.AllocateCores(asid, scaling, serial_runtime);
    SendIPCMessage(msg, /*blocking*/true);

    /* Here we've finished with the allocation decision. */
    int allocation = GetProcessCoreAllocation(asid);
#ifdef ALLOCATOR_DEBUG
    cerr << "ASID: " << asid << " allocated " << allocation << " cores." << endl;
#endif
    ASSERTX(allocation > 0);
    *allocated_cores = allocation;
}

/* ========================================================================== */
VOID ILDJIT_startParallelLoop(THREADID tid, ADDRINT ip, ADDRINT loop, ADDRINT rc)
{
    if(reached_start_invocation) {
        loop_states.push(loop_state_t());
        loop_state = &(loop_states.top());
        loop_state->simmed_iteration_count = 0;
        loop_state->current_loop = loop;
        loop_state->invocationCount = invocation_counts[(string)(char*)loop];
        loop_state->iterationCount = -1;

        CHAR* loop_name = (CHAR*) loop;
        cerr << "Starting loop: " << loop_name << "[" << invocation_counts[(string)(char*)loop] << "]" << endl;

        *ss_curr = rc;
        loop_state->use_ring_cache = (rc > 0);

        if(disable_wait_signal) {
            loop_state->use_ring_cache = false;
        }
    }

    // If we didn't get to the start of the phase, return
    if(!reached_start_iteration) {
        return;
    }

    assert(reached_start_iteration);
    ignoreSignalZero = false;

    initializePerThreadLoopState(tid);
    simulating_parallel_loop = true;

    if (ExecMode != EXECUTION_MODE_SIMULATE) {
        if(KnobWarmLLC.Value()) {
            assert(reached_warm_invocation);
        }    
        else {    
            cerr << "Do late!" << endl;
            doLateILDJITInstrumentation();
            cerr << "Done late!" << endl;
        }    

        cerr << "FastForward runtime:";
        printElapsedTime();

        cerr << "Starting simulation, TID: " << tid << endl;
        PPointHandler(CONTROL_START, NULL, NULL, NULL, tid);
        first_invocation = false;
    }
    else {
        cerr << tid << ": resuming simulation" << endl;
    }
    ILDJIT_ResumeSimulation(tid);
}
/* ========================================================================== */

// Assumes that start iteration calls _always_ happen in sequential order,
// in a thread safe manner!  Have to take Simon's word on this one for now...
// Must be called from within the body of MOLECOOL_beforeWait!
VOID ILDJIT_startIteration(THREADID tid)
{
    if(KnobWarmLLC.Value()) {
        if(!reached_warm_invocation) {
            assert(!reached_start_invocation);
            assert(!reached_end_invocation);
            return;
        }
    }

    if((!reached_start_invocation) && (!reached_end_invocation)) {
        return;
    }

    assert(loop_state != NULL);
    loop_state->iterationCount++;

    // Check if this is the first iteration
    if((!reached_start_iteration) && loopMatches(start_loop, start_loop_invocation, start_loop_iteration)) {
        cerr << "FastForward runtime:";
        printElapsedTime();

        reached_start_iteration = true;
        loop_state->simmed_iteration_count = 0;

        if(KnobWarmLLC.Value()) {
            assert(reached_warm_invocation);
        }
        else {
            cerr << "Do late!" << endl;
            doLateILDJITInstrumentation();
            cerr << "Done late!" << endl;
        }

        cerr << "Starting simulation, TID: " << tid << endl;
        initializePerThreadLoopState(tid);

        simulating_parallel_loop = true;
        PPointHandler(CONTROL_START, NULL, NULL, NULL, tid);
        ILDJIT_ResumeSimulation(tid);
    }

    // Check if this is the last iteration
    if(reached_end_iteration || loopMatches(end_loop, end_loop_invocation, end_loop_iteration)) {
        assert(reached_start_invocation && reached_end_invocation && reached_start_iteration);
        shutdownSimulation(tid);
    }

    if(reached_start_iteration) {
        assert(reached_start_invocation);
        loop_state->simmed_iteration_count++;
    }
}

/* ========================================================================== */
VOID ILDJIT_endParallelLoop(THREADID tid, ADDRINT loop, ADDRINT numIterations)
{
#ifdef ZESTO_PIN_DBG
    cerr << tid << ": Pausing simulation!" << endl;
#endif
    if (ExecMode == EXECUTION_MODE_SIMULATE) {
        if(reached_end_invocation) {
            cerr << tid << ": Shutting down early!" << endl;
            shutdownSimulation(tid);
        }

        ILDJIT_PauseSimulation(tid);
        cerr << tid << ": Paused simulation!" << endl;

        first_invocation = false;

        list<THREADID>::iterator it;
        ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
            thread_state_t* tstate = get_tls(*it);
            lk_lock(&tstate->lock, tid+1);
            tstate->ignore = true;
            tstate->pop_loop_state();
            lk_unlock(&tstate->lock);
        }

        CHAR* loop_name = (CHAR*) loop;
        UINT32 iterCount = loop_state->simmed_iteration_count - 1;
        cerr << "Ending loop: " << loop_name << " NumIterations:" << iterCount << endl;
        simulating_parallel_loop = false;
        *ss_prev = *ss_curr;

        assert(loop_states.size() > 0);
        loop_states.pop();
        if(loop_states.size() > 0) {
            loop_state = &(loop_states.top());
        }
    }
}

/* ========================================================================== */
VOID ILDJIT_beforeWait(THREADID tid, ADDRINT ssID, ADDRINT pc, ADDRINT retPC)
{
#ifdef PRINT_WAITS
    lk_lock(printing_lock, tid+1);
    if (ExecMode == EXECUTION_MODE_SIMULATE)
        cerr << tid <<" :Before Wait "<< hex << pc << dec  << " ID: " << dec << ssID << endl;
    lk_unlock(printing_lock);
#endif

    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = true;
    lk_unlock(&tstate->lock);

    if(KnobNumCores.Value() == 1) {
        return;
    }

    tstate->lastWaitID = ssID;
    tstate->retPC = retPC;
}

/* ========================================================================== */
VOID ILDJIT_afterWait(THREADID tid, ADDRINT ssID, ADDRINT is_light, ADDRINT pc, ADDRINT esp_val)
{
    assert(ssID < 1024);
    thread_state_t* tstate = get_tls(tid);
    handshake_container_t *handshake, *handshake_2;
    bool first_insn;
    INT32 wait_address;

    if (ExecMode != EXECUTION_MODE_SIMULATE)
        goto cleanup;

    lk_lock(&tstate->lock, tid+1);

    tstate->ignore = false;

#ifdef PRINT_WAITS
    lk_lock(printing_lock, tid+1);
    if (ExecMode == EXECUTION_MODE_SIMULATE)
      cerr << tid <<": After Wait "<< hex << pc << dec  << " ID: " << tstate->lastWaitID << ":" << ssID << endl;
    lk_unlock(printing_lock);
#endif

    // Indicates not in a wait any more
    tstate->lastWaitID = -1;

    first_insn = tstate->firstInstruction;

    /* Not simulating -- just ignore. */
    if (tstate->ignore_all) {
        lk_unlock(&tstate->lock);
        goto cleanup;
    }

    lk_unlock(&tstate->lock);

    /* Don't insert waits in single-core mode */
    if (KnobNumCores.Value() == 1)
        goto cleanup;

    tstate->loop_state->unmatchedWaits++;
    assert(loop_state->simmed_iteration_count > 0);

    // If ring cache is disabled, don't insert waits
    if(!(loop_state->use_ring_cache)) {
        goto cleanup;
    }

    // If prologue wait is light, we can ignore it and the
    // subsequent signal, according to Simone
    if(ssID == 0 && is_light && !coupled_waits) {
        ignoreSignalZero = true;
        goto cleanup;
    }
    
    /* Ignore injecting waits until the end of the first iteration,
     * so we can start simulating */
    if (loop_state->simmed_iteration_count == 1) {
      goto cleanup;
    }
    
    // Don't insert light waits into pipeline
    if(!coupled_waits && !insert_light_waits && is_light) {
        goto cleanup;
    }

    /* We're not a first instruction any more */
    if (first_insn) {
        lk_lock(&tstate->lock, tid+1);
        tstate->firstInstruction = false;
        lk_unlock(&tstate->lock);
    }

    /* Insert wait instruction in pipeline */
    handshake = xiosim::buffer_management::get_buffer(tstate->tid);

    handshake->flags.isFirstInsn = first_insn;
    handshake->handshake.ctxt.regs_R.dw[MD_REG_ESP] = esp_val; /* Needed when first_insn to set up stack pages */
    handshake->flags.sleep_thread = false;
    handshake->flags.resume_thread = false;
    handshake->flags.real = false;
    handshake->flags.in_critical_section = true;
    handshake->handshake.asid = asid;
    handshake->flags.valid = true;

    handshake->handshake.pc = (ADDRINT)wait_template_1;
    handshake->handshake.npc = (ADDRINT)wait_template_2;
    handshake->handshake.tpc = (ADDRINT)wait_template_2;
    handshake->flags.brtaken = false;
    memcpy(handshake->handshake.ins, wait_template_1, wait_template_1_size);
    wait_address = getSignalAddress(ssID) | HELIX_WAIT_MASK |
                        (is_light ? HELIX_LIGHT_WAIT_MASK : 0);
    *(INT32*)(&handshake->handshake.ins[wait_template_1_addr_offset]) = wait_address;
    assert(ssID < HELIX_MAX_SIGNAL_ID);

#ifdef PRINT_DYN_TRACE
    printTrace("sim", handshake->handshake.pc, tstate->tid);
#endif

    xiosim::buffer_management::producer_done(tstate->tid);

    handshake_2 = xiosim::buffer_management::get_buffer(tstate->tid);
    handshake_2->flags.real = false;
    handshake_2->flags.in_critical_section = true;
    handshake_2->handshake.asid = asid;
    handshake_2->flags.valid = true;

    handshake_2->handshake.pc = (ADDRINT)wait_template_2;
    handshake_2->handshake.npc = NextUnignoredPC(tstate->retPC);
    handshake_2->handshake.tpc = (ADDRINT)wait_template_2 + wait_template_2_size;
    handshake_2->flags.brtaken = false;
    memcpy(handshake_2->handshake.ins, wait_template_2, wait_template_2_size);

#ifdef PRINT_DYN_TRACE
    printTrace("sim", handshake_2->handshake.pc, tstate->tid);
#endif

    xiosim::buffer_management::producer_done(tstate->tid);

cleanup:
    tstate->lastSignalAddr = 0xdecafbad;
}

/* ========================================================================== */
VOID ILDJIT_beforeSignal(THREADID tid, ADDRINT ssID, ADDRINT pc, ADDRINT retPC)
{
    thread_state_t* tstate = get_tls(tid);

    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = true;
    lk_unlock(&tstate->lock);
#ifdef PRINT_WAITS
    lk_lock(printing_lock, tid+1);
    if (ExecMode == EXECUTION_MODE_SIMULATE)
        cerr << tid <<": Before Signal " << hex << pc << " ID: " << ssID << dec << endl;
    lk_unlock(printing_lock);
#endif

//    ASSERTX(tstate->lastSignalAddr == 0xdecafbad);
    tstate->retPC = retPC;
}

/* ========================================================================== */
VOID ILDJIT_afterSignal(THREADID tid, ADDRINT ssID, ADDRINT pc)
{
    assert(ssID < 1024);
    thread_state_t* tstate = get_tls(tid);
    handshake_container_t* handshake;

    /* Not simulating -- just ignore. */
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        goto cleanup;


    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = false;

    /* Not simulating -- just ignore. */
    if (tstate->ignore_all) {
        lk_unlock(&tstate->lock);
        goto cleanup;
    }

    assert(!tstate->firstInstruction);
    lk_unlock(&tstate->lock);

#ifdef PRINT_WAITS
    lk_lock(printing_lock, tid+1);
    if (ExecMode == EXECUTION_MODE_SIMULATE)
        cerr << tid <<": After Signal " << hex << pc << dec << endl;
    lk_unlock(printing_lock);
#endif


    /* Don't insert signals in single-core mode */
    if (KnobNumCores.Value() == 1)
        goto cleanup;

    tstate->loop_state->unmatchedWaits--;
    ASSERTX(tstate->loop_state->unmatchedWaits >= 0);

    if(!(loop_state->use_ring_cache)) {
        return;
    }

    if(ssID == 0 && ignoreSignalZero) {
      goto cleanup;
    }

    /* Insert signal instruction in pipeline */
    handshake = xiosim::buffer_management::get_buffer(tstate->tid);

    handshake->flags.isFirstInsn = false;
    handshake->flags.sleep_thread = false;
    handshake->flags.resume_thread = false;
    handshake->flags.real = false;
    handshake->flags.in_critical_section = (tstate->loop_state->unmatchedWaits > 0);
    handshake->handshake.asid = asid;
    handshake->flags.valid = true;

    handshake->handshake.pc = (ADDRINT)signal_template;
    handshake->handshake.npc = NextUnignoredPC(tstate->retPC);
    handshake->handshake.tpc = (ADDRINT)signal_template + sizeof(signal_template);
    handshake->flags.brtaken = false;
    memcpy(handshake->handshake.ins, signal_template, sizeof(signal_template));
    // Address comes right after opcode and MoodRM bytes
    *(INT32*)(&handshake->handshake.ins[2]) = getSignalAddress(ssID);
    assert(ssID < HELIX_MAX_SIGNAL_ID);

#ifdef PRINT_DYN_TRACE
    printTrace("sim", handshake->handshake.pc, tstate->tid);
#endif

    xiosim::buffer_management::producer_done(tstate->tid);

cleanup:
    tstate->lastSignalAddr = 0xdecafbad;
}

/* ========================================================================== */
VOID ILDJIT_setAffinity(THREADID tid, INT32 coreID)
{
#ifdef ZESTO_PIN_DBG
    thread_state_t* tstate = get_tls(tid);
    lk_lock(printing_lock, 1);
    cerr << "Call to setAffinity: " << tstate->tid << " " << coreID << endl;
    lk_unlock(printing_lock);
#endif

    lk_lock(&lk_affine_threads, 1);
    virtual_affinity[tid] = coreID;

    /* Construct array of thread ids, ordered by virtual core affinity. */
    auto it = affine_threads.begin();
    for (/*nada*/; it != affine_threads.end(); it++) {
        if (virtual_affinity[*it] > coreID)
            break;
    }
    affine_threads.insert(it, tid);
    lk_unlock(&lk_affine_threads);
}

/* ========================================================================== */
VOID AddILDJITCallbacks(IMG img)
{
#ifdef ZESTO_PIN_DBG
    cerr << "Adding ILDJIT callbacks: ";
#endif

    //Interface to ildjit
    RTN rtn;
    rtn = RTN_FindByName(img, "MOLECOOL_codeExecutorCreation");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_codeExecutorCreation ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_ExecutorCreate),
                       IARG_THREAD_ID,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_init");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_init ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_setupInterface),
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_startIteration");
    if (RTN_Valid(rtn))
    {
        fprintf(stderr, "MOLECOOL_startIteration(): %p\n", RTN_Funptr(rtn));
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_startIteration ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_startIteration),
                       IARG_THREAD_ID,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_beforeWait");
    if (RTN_Valid(rtn))
    {
        fprintf(stderr, "MOLECOOL_beforeWait(): %p\n", RTN_Funptr(rtn));
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_beforeWait),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_INST_PTR,
                       IARG_RETURN_IP, // Only valid here, not on after*
                       IARG_CALL_ORDER, CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
        IgnoreCallsTo(RTN_Address(rtn), 2/*the call and one parameter*/, (ADDRINT)wait_template_1);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_afterWait");
    if (RTN_Valid(rtn))
    {
        fprintf(stderr, "MOLECOOL_afterWait(): %p\n", RTN_Funptr(rtn));
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(ILDJIT_afterWait),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_INST_PTR,
                       IARG_REG_VALUE, LEVEL_BASE::REG_ESP,
                       IARG_CALL_ORDER, CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
        IgnoreCallsTo(RTN_Address(rtn), 3/*the call and two parameters*/, (ADDRINT)-1);
        pc_diss[(ADDRINT)wait_template_1] = "Wait";
    }

    rtn = RTN_FindByName(img, "MOLECOOL_beforeSignal");
    if (RTN_Valid(rtn))
    {
        fprintf(stderr, "MOLECOOL_beforeSignal(): %p\n", RTN_Funptr(rtn));
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_beforeSignal),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_INST_PTR,
                       IARG_RETURN_IP, // Only valid here, not on after*
                       IARG_CALL_ORDER, CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
        IgnoreCallsTo(RTN_Address(rtn), 2/*the call and one parameter*/, (ADDRINT)signal_template);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_afterSignal");
    if (RTN_Valid(rtn))
    {
        fprintf(stderr, "MOLECOOL_afterSignal(): %p\n", RTN_Funptr(rtn));
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(ILDJIT_afterSignal),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_INST_PTR,
                       IARG_CALL_ORDER, CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
        IgnoreCallsTo(RTN_Address(rtn), 2/*the call and one parameter*/, (ADDRINT)-1);
        pc_diss[(ADDRINT)signal_template] = "Signal";
    }

    rtn = RTN_FindByName(img, "MOLECOOL_endParallelLoop");
    if (RTN_Valid(rtn))
    {
        fprintf(stderr, "MOLECOOL_endParallelLoop(): %p\n", RTN_Funptr(rtn));
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_endParallelLoop),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_END);
        RTN_Close(rtn);
        IgnoreCallsTo(RTN_Address(rtn), 3/*the call and two parameters*/, (ADDRINT)-1);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_codeExecutorCreationEnd");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_codeExecutorCreationEnd ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_ExecutorCreateEnd),
                       IARG_THREAD_ID,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_startInitParallelLoop");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_startInitParallelLoop ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(ILDJIT_startInitParallelLoop),
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_startParallelLoop");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_startParallelLoop ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(ILDJIT_startParallelLoop),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_startLoop");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_startLoop ";
#endif
        fprintf(stderr, "MOLECOOL_startLoop(): %p\n", RTN_Funptr(rtn));
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_startLoop),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_CALL_ORDER, CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
        IgnoreCallsTo(RTN_Address(rtn), 2/*the call and one parameter*/, (ADDRINT)-1);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_startLoop");
    if (RTN_Valid(rtn))
      {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_startLoop ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(ILDJIT_startLoop_after),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_CALL_ORDER, CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }



    rtn = RTN_FindByName(img, "MOLECOOL_startSimulation");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_startSimulation ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_startSimulation),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_endSimulation");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_endSimulation ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(ILDJIT_endSimulation),
                       IARG_THREAD_ID,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "MOLECOOL_setAffinity");
    if (RTN_Valid(rtn))
    {
#ifdef ZESTO_PIN_DBG
        cerr << "MOLECOOL_setAffinity ";
#endif
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(ILDJIT_setAffinity),
                       IARG_THREAD_ID,
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_CALL_ORDER, CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
    }

//==========================================================
//FLUFFY-related
    if (!KnobFluffy.Value().empty())
    {

        rtn = RTN_FindByName(img, "step3_start_inst");
        if (RTN_Valid(rtn))
        {
#ifdef ZESTO_PIN_DBG
            cerr << "FLUFFY_step3_start_inst ";
#endif
            RTN_Open(rtn);

            RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(FLUFFY_StartInsn),
                           IARG_THREAD_ID,
                           IARG_INST_PTR,
                           IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                           IARG_END);

            RTN_Close(rtn);
        }

        rtn = RTN_FindByName(img, "step3_end_inst");
        if (RTN_Valid(rtn))
        {
#ifdef ZESTO_PIN_DBG
            cerr << "FLUFFY_step3_end_inst ";
#endif
            RTN_Open(rtn);

            RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(FLUFFY_StopInsn),
                           IARG_THREAD_ID,
                           IARG_INST_PTR,
                           IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                           IARG_END);

            RTN_Close(rtn);
        }
    }
#ifdef ZESTO_PIN_DBG
    cerr << endl;
#endif
}

UINT32 getSignalAddress(ADDRINT ssID)
{
    CoreSet allocatedCores = GetProcessCoreSet(asid);
    assert(allocatedCores.size());

    /* We need the first core that starts the loop invocation in order
     * to initialize the signal cache differently (so cores don't wait
     * on non-existent iterations). */
    int offsetFromFirst = 0;
    if (first_invocation) {
    /* In the majority of cases, the first thread of the invocation starts
     * the first iteration. Except, if we start sampling mid-invocation.
     * In that case, we assume iterations are distributed to the allocated
     * core set in increasing order. */
        offsetFromFirst = start_loop_iteration % allocatedCores.size();
    }

    auto it = allocatedCores.begin();
    for (int i=0; i < offsetFromFirst; i++)
        it++;

    int firstCore = *it;

    assert(firstCore < MAX_CORES);
    assert(ssID <= HELIX_MAX_SIGNAL_ID);

    return 0x7ffc0000 + (firstCore << HELIX_SIGNAL_FIRST_CORE_SHIFT) + ssID;
}

bool loopMatches(string loop, UINT32 invocationNum, UINT32 iterationNum)
{
    if(loop_state->invocationCount != invocationNum) {
        return false;
    }

    if(loop_state->iterationCount != iterationNum) {
        return false;
    }

    if(string((char*)loop_state->current_loop) != loop) {
        return false;
    }

    return true;
}

void initializePerThreadLoopState(THREADID tid)
{
    list<THREADID>::iterator it;
    ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
        thread_state_t* curr_tstate = get_tls(*it);
        lk_lock(&curr_tstate->lock, tid+1);
        curr_tstate->push_loop_state();
        lk_unlock(&curr_tstate->lock);
    }
}

void readLoop(ifstream& fin, string* name, UINT32* invocation, UINT32* iteration)
{
    string line;

    getline(fin, *name);

    getline(fin, line);
    *invocation = Uint32FromString(line);
    assert((UINT64)(*invocation) == Uint64FromString(line));

    getline(fin, line);
    if(line == "-1") {
        *iteration = -1;
    }
    else {
        *iteration = Uint32FromString(line);
        assert((UINT64)(*iteration) == Uint64FromString(line));
    }
}

/* ========================================================================== */
VOID ILDJIT_PauseSimulation(THREADID tid)
{
    /* The context is that all cores functionally have sent signal 0
     * and unblocked the last iteration. We need to (i) wait for them
     * to functionally reach wait 0, where they will wait until the end
     * of the loop; (ii) drain all pipelines once cores are waiting. */
    lk_lock(printing_lock, tid);
    cerr << "pausing..." << endl;
    lk_unlock(printing_lock);

    volatile bool done_with_iteration = false;
    do {
        done_with_iteration = true;
        list<THREADID>::iterator it;
        ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
            if ((*it) != tid) {
                thread_state_t* tstate = get_tls(*it);
                lk_lock(&tstate->lock, 1);
                bool curr_done = tstate->ignore_all ||
                    (tstate->ignore && (tstate->lastWaitID == 0));
                done_with_iteration &= curr_done;
                /* Setting ignore_all here (while ignore is set) should be a race-free way
                 * of ignoring the serial portion outside the loop after the thread goes
                 * on an unsets ignore locally. */
                if (curr_done) {
                    tstate->ignore_all = true;
                }
                lk_unlock(&tstate->lock);
            }
        }
    } while (!done_with_iteration);

    /* Here we have produced everything for this loop! */
    disable_producers();
    enable_consumers();

    /* Drainning all pipelines and deactivating cores. */
    vector<THREADID>::iterator it;
    unsigned int thread_count = 0;
    ATOMIC_ITERATE(affine_threads, it, lk_affine_threads) {
        auto curr_tstate = get_tls(*it);
        /* Insert a special signal that flushes the repeater. */
        handshake_container_t* handshake_0 = xiosim::buffer_management::get_buffer(curr_tstate->tid);

        handshake_0->flags.real = false;
        handshake_0->handshake.asid = asid;
        handshake_0->flags.valid = true;

        handshake_0->handshake.pc = (ADDRINT)signal_template;
        handshake_0->handshake.npc = (ADDRINT)mfence_template;
        handshake_0->handshake.tpc = (ADDRINT)signal_template + sizeof(signal_template);
        handshake_0->flags.brtaken = false;
        memcpy(handshake_0->handshake.ins, signal_template, sizeof(signal_template));
        // Address comes right after opcode and MoodRM bytes
        *(INT32*)(&handshake_0->handshake.ins[2]) = getSignalAddress(HELIX_FLUSH_SIGNAL_ID);
        xiosim::buffer_management::producer_done(curr_tstate->tid, true);

        /* Insert a MFENCE. This makes sure that all operations to the repeater
         * have not only been scheduled, but also completed and ack-ed. */
        handshake_container_t* handshake = xiosim::buffer_management::get_buffer(curr_tstate->tid);
        handshake->flags.real = false;
        handshake->handshake.asid = asid;
        handshake->flags.valid = true;

        handshake->handshake.pc = (ADDRINT) mfence_template;
        handshake->handshake.npc = (ADDRINT)syscall_template;
        handshake->handshake.tpc = (ADDRINT) mfence_template + sizeof(mfence_template);
        handshake->flags.brtaken = false;
        memcpy(handshake->handshake.ins, mfence_template, sizeof(mfence_template));
        xiosim::buffer_management::producer_done(curr_tstate->tid, true);

        /* Insert a trap. This will ensure that the pipe drains before
         * consuming the next instruction.*/
        handshake_container_t* handshake_1 = xiosim::buffer_management::get_buffer(curr_tstate->tid);
        handshake_1->flags.real = false;
        handshake_1->handshake.asid = asid;
        handshake_1->flags.valid = true;

        handshake_1->handshake.pc = (ADDRINT) syscall_template;
        handshake_1->handshake.npc = (ADDRINT) syscall_template + sizeof(syscall_template);
        handshake_1->handshake.tpc = (ADDRINT) syscall_template + sizeof(syscall_template);
        handshake_1->flags.brtaken = false;
        memcpy(handshake_1->handshake.ins, syscall_template, sizeof(syscall_template));
        xiosim::buffer_management::producer_done(curr_tstate->tid, true);

        /* And finally, flush the core's pipelie to get rid of anything
         * left over (including the trap). */
        handshake_container_t* handshake_3 = xiosim::buffer_management::get_buffer(curr_tstate->tid);

        handshake_3->flags.flush_pipe = true;
        handshake_3->flags.real = false;
        handshake_3->handshake.asid = asid;
        handshake_3->handshake.pc = 0;
        handshake_3->flags.valid = true;
        xiosim::buffer_management::producer_done(curr_tstate->tid, true);

        /* Let the scheduler deactivate this core. */
        handshake_container_t* handshake_2 = xiosim::buffer_management::get_buffer(curr_tstate->tid);

        handshake_2->flags.real = false;
        handshake_2->handshake.asid = asid;
        handshake_2->flags.giveCoreUp = true;
        handshake_2->flags.giveUpReschedule = false;
        handshake_2->flags.valid = true;
        xiosim::buffer_management::producer_done(curr_tstate->tid, true);

        xiosim::buffer_management::flushBuffers(curr_tstate->tid);

        /* All other threads didn't participate in the loop */
        thread_count++;
        if (thread_count == *allocated_cores)
            break;
    }

    enable_consumers();

    /* Wait until all cores are done -- consumed their buffers. */
    volatile bool done = false;
    do {
        done = true;
        vector<THREADID>::iterator it;
        ATOMIC_ITERATE(affine_threads, it, lk_affine_threads) {
            auto curr_tstate = get_tls(*it);
            done &= !IsSHMThreadSimulatingMaybe(curr_tstate->tid);
        }
        if (!done && *sleeping_enabled)
            PIN_Sleep(10);
    } while (!done);

#ifdef ZESTO_PIN_DBG
    cerr << tid << " [KEVIN]: All cores have empty buffers" << endl;
    cerr.flush();
#endif

#if 0
    tick_t most_cycles = 0;
    ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
        auto curr_tstate = get_tls(*it);
        int coreID = GetSHMThreadCore(curr_tstate->tid);
        if(cores[coreID]->sim_cycle > most_cycles) {
            most_cycles = cores[coreID]->sim_cycle;
        }
    }

    ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
        auto curr_tstate = get_tls(*it);
        int coreID = GetSHMThreadCore(curr_tstate->tid);
        assert(tstate != NULL);
        cores[coreID]->sim_cycle = most_cycles;
        cerr << coreID << ":OverlapCycles:" << most_cycles - lastConsumerApply[*it] << endl;
    }
#endif

    disable_consumers();
    enable_producers();

    /* Have thread ignore serial section after */
    /* XXX: Do we need this? A few lines above we set ignore_all! */
    ATOMIC_ITERATE(affine_threads, it, lk_affine_threads) {
        thread_state_t* tstate = get_tls(*it);
        lk_lock(&tstate->lock, 1);
        xiosim::buffer_management::resetPool(tstate->tid);
        tstate->ignore = true;
        lk_unlock(&tstate->lock);
        assert(xiosim::buffer_management::empty(tstate->tid));
    }

    CoreSet empty_set;
    UpdateProcessCoreSet(asid, empty_set);

    ipc_message_t msg;
    msg.DeallocateCores(asid);
    SendIPCMessage(msg, /*blocking*/true);
}

/* ========================================================================== */
VOID ILDJIT_ResumeSimulation(THREADID tid)
{
    UINT32 num_allocated_cores = *allocated_cores;
    list<pid_t> scheduled_threads;

    /* Pick the first @allocated_cores threads, based on virtual affinity. */
    vector<THREADID>::iterator it;
    ATOMIC_ITERATE(affine_threads, it, lk_affine_threads) {
        thread_state_t* tstate = get_tls(*it);
        ASSERTX(xiosim::buffer_management::empty(tstate->tid));

        /* Make sure thread starts producing instructions */
        lk_lock(&tstate->lock, 1);
        tstate->ignore_all = false;
        lk_unlock(&tstate->lock);

        /* This lucky thread will get scheduled */
        scheduled_threads.push_back(tstate->tid);

        if (scheduled_threads.size() == num_allocated_cores)
            break;
    }

    /* Ok, now let the scheduler schedule all these threads. */
    ipc_message_t msg;
    msg.ScheduleProcessThreads(asid, scheduled_threads);
    SendIPCMessage(msg);

    /* Wait until all cores have been scheduled. 
     * Since there is no guarantee when IPC messages are consumed, not
     * waiting can cause a fast thread to race to ILDJIT_PauseSimulation,
     * before everyone has been scheduled to run. */
    volatile bool done = false;
    do {
        done = true;
        vector<THREADID>::iterator it;
        unsigned int thread_count = 0;
        ATOMIC_ITERATE(affine_threads, it, lk_affine_threads) {
            auto curr_tstate = get_tls(*it);
            done &= IsSHMThreadSimulatingMaybe(curr_tstate->tid);

            thread_count++;
            if (thread_count == *allocated_cores)
                break;
        }
    } while (!done);
}

/* ========================================================================== */
VOID shutdownSimulation(THREADID tid)
{
    ILDJIT_PauseSimulation(tid);
    int iterCount = loop_state->simmed_iteration_count - 1;
    cerr << "Ending loop: anonymous" << " NumIterations:" << iterCount << endl;

    cerr << "Simulation runtime:";
    printElapsedTime();
    cerr << "[KEVIN] Stopped simulation! " << tid << endl;
    PIN_ExitProcess(EXIT_SUCCESS);
}
