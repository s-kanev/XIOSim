/* ========================================================================== */
/* ========================================================================== */
/*
 * Molecool: Feeder to Zesto, fed itself by ILDJIT.
 * Copyright, Vijay Reddi, 2007 -- SimpleScalar feeder prototype
              Svilen Kanev, 2011
*/
/* ========================================================================== */
/* ========================================================================== */

#include <iostream>
#include <iomanip>
#include <map>
#include <queue>
#include <set>
#include <list>
#include <syscall.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <stdlib.h>
#include <elf.h>
#include <sched.h>

#include <unistd.h>

#ifdef TIME_TRANSPARENCY
#include "rdtsc.h"
#endif

#include "feeder.h"
#include "ildjit.h"
#include "BufferManager.h"

using namespace std;

/* ========================================================================== */
/* ========================================================================== */
/*                           ZESTO and PIN INTERFACE                          */
/* ========================================================================== */
/* ========================================================================== */

CHAR sim_name[] = "Zesto";

KNOB<string> KnobInsTraceFile(KNOB_MODE_WRITEONCE,   "pintool",
        "trace", "", "File where instruction trace is written");
KNOB<string> KnobSanityInsTraceFile(KNOB_MODE_WRITEONCE,   "pintool",
        "sanity_trace", "", "Instruction trace file to use for sanity checking of codepaths");
KNOB<BOOL> KnobSanity(KNOB_MODE_WRITEONCE,     "pintool",
        "sanity", "false", "Sanity-check if simulator corrupted memory (expensive)");
KNOB<BOOL> KnobILDJIT(KNOB_MODE_WRITEONCE,      "pintool",
        "ildjit", "false", "Application run is ildjit");
KNOB<BOOL> KnobAMDHack(KNOB_MODE_WRITEONCE,      "pintool",
        "amd_hack", "false", "Using AMD syscall hack for use with hpc cluster");
KNOB<string> KnobFluffy(KNOB_MODE_WRITEONCE,      "pintool",
        "fluffy_annotations", "", "Annotation file that specifies fluffy ROI");
KNOB<BOOL> KnobPipelineInstrumentation(KNOB_MODE_WRITEONCE, "pintool",
        "pipeline_instrumentation", "false", "Overlap instrumentation and simulation threads (still unstable)");
KNOB<BOOL> KnobWarmLLC(KNOB_MODE_WRITEONCE,      "pintool",
        "warm_llc", "false", "Warm LLC while fast-forwarding");
KNOB<BOOL> KnobGreedyCores(KNOB_MODE_WRITEONCE, "pintool",
        "greedy_cores", "false", "Spread on all available machine cores");

map<ADDRINT, UINT8> sanity_writes;
map<ADDRINT, string> pc_diss;
BOOL sim_release_handshake;

#ifdef TIME_TRANSPARENCY
// Tracks the time we spend in simulation and tries to subtract it from timing calls
UINT64 sim_time = 0;
#endif

ofstream pc_file;
ofstream trace_file;
ifstream sanity_trace;

// Used to access thread-local storage
static TLS_KEY tls_key;
static XIOSIM_LOCK test;

// Used to access Zesto instruction buffer
PIN_RWMUTEX simbuffer_lock;
map<THREADID, Buffer> lookahead_buffer;
BufferManager handshake_buffer;
vector<THREADID> thread_list;

// Used to manage internal pin simulator threads
static queue<THREADID> instrument_tid_queue;
static XIOSIM_LOCK instrument_tid_lock;

// A mapping storing which thread runs on which core
map<UINT32, THREADID> core_threads;

// The reverse mapping (also stored in tstate_t, but that's supposed to be thread-private)
map<THREADID, UINT32> thread_cores;

// Runque for threads (managed in FIFO order for simple fair scheduling)
static list<THREADID> run_queue;
bool producers_sleep = false;
bool consumers_sleep = false;
INT32 host_cpus;
/* ========================================================================== */
/* Pinpoint related */
// Track the number of instructions executed
ICOUNT icount;

// Contains knobs and instrumentation to recognize start/stop points
CONTROL control;

EXECUTION_MODE ExecMode = EXECUTION_MODE_INVALID;
map<THREADID, tick_t> lastConsumerApply;

typedef pair <UINT32, CHAR **> SSARGS;

// Function to access thread-specific data
/* ========================================================================== */
thread_state_t* get_tls(THREADID threadid)
{
    thread_state_t* tstate =
          static_cast<thread_state_t*>(PIN_GetThreadData(tls_key, threadid));
    return tstate;
}

/* ========================================================================== */
VOID ImageUnload(IMG img, VOID *v)
{
    ADDRINT start = IMG_LowAddress(img);
    ADDRINT length = IMG_HighAddress(img) - start;

#ifdef ZESTO_PIN_DBG
    cerr << "Image unload, addr: " << hex << start
         << " len: " << length << " end_addr: " << start + length << endl;
#endif

    ASSERTX( Zesto_Notify_Munmap(0/*coreID*/, start, length, true));
}

/* ========================================================================== */
VOID PPointHandler(CONTROL_EVENT ev, VOID * v, CONTEXT * ctxt, VOID * ip, THREADID tid)
{
    cerr << "tid: " << dec << tid << " ip: " << hex << ip << " ";
    if (tid < ISIMPOINT_MAX_THREADS)
        cerr <<  dec << " Inst. Count " << icount.Count(tid) << " ";

    thread_state_t* tstate = get_tls(tid);
    volatile handshake_container_t* handshake;
    vector<THREADID>::iterator it;

    switch(ev)
    {
      case CONTROL_START:
        cerr << "Start" << endl;
        ExecMode = EXECUTION_MODE_SIMULATE;
        CODECACHE_FlushCache();
//        PIN_RemoveInstrumentation();
        for (it = thread_list.begin(); it != thread_list.end(); it++) {
            thread_state_t* tstate = get_tls(*it);
            lk_lock(&tstate->lock, tid+1);
            tstate->ignore_all = false;
            lk_unlock(&tstate->lock);
        }

        //ScheduleRunQueue();
        if(control.PinPointsActive())
        {
            cerr << "PinPoint: " << control.CurrentPp(tid) << " PhaseNo: " << control.CurrentPhase(tid) << endl;
        }
        break;

      case CONTROL_STOP:
        cerr << "Stop" << endl;
        if(control.PinPointsActive())
        {
            ExecMode = EXECUTION_MODE_FASTFORWARD;
            CODECACHE_FlushCache();
    //        PIN_RemoveInstrumentation();
            // XXX: This should be simpler to do now! Fix me!
            PIN_RWMutexWriteLock(&simbuffer_lock);
            /* Update thread state while holding the simbuffer lock.
             * There are two orderings: (1) this is executed before
             * instrumenting the instruction marked as a Stop point.
             * Then, updating tstate is enough. */
            tstate->slice_num = control.CurrentPp(tid);
            tstate->slice_length = control.CurrentPpLength(tid);
            tstate->slice_weight_times_1000 = control.CurrentPpWeightTimesThousand(tid);
            /* (2) This instrumentation routine is executed after
             * instrumenting the Stop point. Then, we need to update
             * the handshake buffer that gets passed to the simulator
             * directly.
             * In either case, this is executed before SimulateLoop on
             * the Stop point? */

            int slice_num = tstate->slice_num;
            int slice_length = tstate->slice_length;
            int slice_weight_times = tstate->slice_weight_times_1000;

	    assert(lookahead_buffer[tid].size() == 0);
            handshake = handshake_buffer.front(tid);

            handshake->handshake.slice_num = slice_num;
            handshake->handshake.feeder_slice_length = slice_length;
            handshake->handshake.slice_num = slice_weight_times;

            handshake->flags.isLastInsn = true;

            PIN_RWMutexUnlock(&simbuffer_lock);

            cerr << "PinPoint: " << control.CurrentPp(tid) << endl;
        }
        else if(KnobFluffy.Value().empty())
        {
            // Should be handled at ThreadFini
            ASSERTX(false);
        }

        break;

      default:
        ASSERTX(false);
        break;
    }
    cerr << "Leaving PPOINT" << endl;
}

/* ========================================================================== */
VOID ImageLoad(IMG img, VOID *v)
{
    ADDRINT start = IMG_LowAddress(img);
    ADDRINT length = IMG_HighAddress(img) - start;

#ifdef ZESTO_PIN_DBG
    cerr << "Image load, addr: " << hex << start
         << " len: " << length << " end_addr: " << start + length << endl;
#endif

    // Register callback interface to get notified on ILDJIT events
    if (KnobILDJIT.Value())
        AddILDJITCallbacks(img);

    ASSERTX( Zesto_Notify_Mmap(0/*coreID*/, start, length, false) );
}

/* ========================================================================== */
/* The last parameter is a  pointer to static data that is overwritten with each call */
VOID MakeSSContext(const CONTEXT *ictxt, FPSTATE* fpstate, ADDRINT pc, ADDRINT npc, regs_t *ssregs)
{
    CONTEXT ssctxt;
    memset(&ssctxt, 0x0, sizeof(ssctxt));
    PIN_SaveContext(ictxt, &ssctxt);

    // Must invalidate prior to use because previous invocation data still
    // resides in this statically allocated buffer
    memset(ssregs, 0x0, sizeof(regs_t));
    ssregs->regs_PC = pc;
    ssregs->regs_NPC = npc;

    // Copy general purpose registers, which Pin provides individual access to
    ssregs->regs_C.aflags = PIN_GetContextReg(&ssctxt, REG_EFLAGS);
    ssregs->regs_R.dw[MD_REG_EAX] = PIN_GetContextReg(&ssctxt, REG_EAX);
    ssregs->regs_R.dw[MD_REG_ECX] = PIN_GetContextReg(&ssctxt, REG_ECX);
    ssregs->regs_R.dw[MD_REG_EDX] = PIN_GetContextReg(&ssctxt, REG_EDX);
    ssregs->regs_R.dw[MD_REG_EBX] = PIN_GetContextReg(&ssctxt, REG_EBX);
    ssregs->regs_R.dw[MD_REG_ESP] = PIN_GetContextReg(&ssctxt, REG_ESP);
    ssregs->regs_R.dw[MD_REG_EBP] = PIN_GetContextReg(&ssctxt, REG_EBP);
    ssregs->regs_R.dw[MD_REG_EDI] = PIN_GetContextReg(&ssctxt, REG_EDI);
    ssregs->regs_R.dw[MD_REG_ESI] = PIN_GetContextReg(&ssctxt, REG_ESI);


    // Copy segment selector registers (IA32-specific)
    ssregs->regs_S.w[MD_REG_CS] = PIN_GetContextReg(&ssctxt, REG_SEG_CS);
    ssregs->regs_S.w[MD_REG_SS] = PIN_GetContextReg(&ssctxt, REG_SEG_SS);
    ssregs->regs_S.w[MD_REG_DS] = PIN_GetContextReg(&ssctxt, REG_SEG_DS);
    ssregs->regs_S.w[MD_REG_ES] = PIN_GetContextReg(&ssctxt, REG_SEG_ES);
    ssregs->regs_S.w[MD_REG_FS] = PIN_GetContextReg(&ssctxt, REG_SEG_FS);
    ssregs->regs_S.w[MD_REG_GS] = PIN_GetContextReg(&ssctxt, REG_SEG_GS);

    // Copy segment base registers (simulator needs them for address calculations)
    // XXX: For security reasons, we (as user code) aren't allowed to touch those.
    // So, we access whatever we can (FS and GS because of 64-bit addressing).
    // For the rest, Linux sets the base of user-leve CS and DS to 0, so life is good.
    // FIXME: Check what Linux does with user-level SS and ES!

    ssregs->regs_SD.dw[MD_REG_FS] = PIN_GetContextReg(&ssctxt, REG_SEG_FS_BASE);
    ssregs->regs_SD.dw[MD_REG_GS] = PIN_GetContextReg(&ssctxt, REG_SEG_GS_BASE);

    // Copy floating purpose registers: Floating point state is generated via
    // the fxsave instruction, which is a 512-byte memory region. Look at the
    // SDM for the complete layout of the fxsave region. Zesto only
    // requires the (1) floating point status word, the (2) fp control word,
    // and the (3) eight 10byte floating point registers. Thus, we only copy
    // the required information into the SS-specific (and Zesto-inherited)
    // data structure
    ASSERTX(PIN_ContextContainsState(&ssctxt, PROCESSOR_STATE_X87));
    PIN_GetContextFPState(ictxt, fpstate);

    //Copy the floating point control word
    memcpy(&ssregs->regs_C.cwd, &fpstate->fxsave_legacy._fcw, 2);

    // Copy the floating point status word
    memcpy(&ssregs->regs_C.fsw, &fpstate->fxsave_legacy._fsw, 2);

    //Copy floating point tag word specifying which regsiters hold valid values
    memcpy(&ssregs->regs_C.ftw, &fpstate->fxsave_legacy._ftw, 1);

    //For Zesto, regs_F is indexed by physical register, not stack-based
    #define ST2P(num) ((FSW_TOP(ssregs->regs_C.fsw) + (num)) & 0x7)

    // Copy actual extended fp registers
    memcpy(&ssregs->regs_F.e[ST2P(MD_REG_ST0)], &fpstate->fxsave_legacy._sts[MD_REG_ST0], MD_FPR_SIZE);
    memcpy(&ssregs->regs_F.e[ST2P(MD_REG_ST1)], &fpstate->fxsave_legacy._sts[MD_REG_ST1], MD_FPR_SIZE);
    memcpy(&ssregs->regs_F.e[ST2P(MD_REG_ST2)], &fpstate->fxsave_legacy._sts[MD_REG_ST2], MD_FPR_SIZE);
    memcpy(&ssregs->regs_F.e[ST2P(MD_REG_ST3)], &fpstate->fxsave_legacy._sts[MD_REG_ST3], MD_FPR_SIZE);
    memcpy(&ssregs->regs_F.e[ST2P(MD_REG_ST4)], &fpstate->fxsave_legacy._sts[MD_REG_ST4], MD_FPR_SIZE);
    memcpy(&ssregs->regs_F.e[ST2P(MD_REG_ST5)], &fpstate->fxsave_legacy._sts[MD_REG_ST5], MD_FPR_SIZE);
    memcpy(&ssregs->regs_F.e[ST2P(MD_REG_ST6)], &fpstate->fxsave_legacy._sts[MD_REG_ST6], MD_FPR_SIZE);
    memcpy(&ssregs->regs_F.e[ST2P(MD_REG_ST7)], &fpstate->fxsave_legacy._sts[MD_REG_ST7], MD_FPR_SIZE);

    ASSERTX(PIN_ContextContainsState(&ssctxt, PROCESSOR_STATE_XMM));

    memcpy(&ssregs->regs_XMM.qw[MD_REG_XMM0].lo, &fpstate->fxsave_legacy._xmms[MD_REG_XMM0], MD_XMM_SIZE);
    memcpy(&ssregs->regs_XMM.qw[MD_REG_XMM1].lo, &fpstate->fxsave_legacy._xmms[MD_REG_XMM1], MD_XMM_SIZE);
    memcpy(&ssregs->regs_XMM.qw[MD_REG_XMM2].lo, &fpstate->fxsave_legacy._xmms[MD_REG_XMM2], MD_XMM_SIZE);
    memcpy(&ssregs->regs_XMM.qw[MD_REG_XMM3].lo, &fpstate->fxsave_legacy._xmms[MD_REG_XMM3], MD_XMM_SIZE);
    memcpy(&ssregs->regs_XMM.qw[MD_REG_XMM4].lo, &fpstate->fxsave_legacy._xmms[MD_REG_XMM4], MD_XMM_SIZE);
    memcpy(&ssregs->regs_XMM.qw[MD_REG_XMM5].lo, &fpstate->fxsave_legacy._xmms[MD_REG_XMM5], MD_XMM_SIZE);
    memcpy(&ssregs->regs_XMM.qw[MD_REG_XMM6].lo, &fpstate->fxsave_legacy._xmms[MD_REG_XMM6], MD_XMM_SIZE);
    memcpy(&ssregs->regs_XMM.qw[MD_REG_XMM7].lo, &fpstate->fxsave_legacy._xmms[MD_REG_XMM7], MD_XMM_SIZE);
}

/* ========================================================================== */
VOID Fini(INT32 exitCode, VOID *v)
{
    Zesto_Destroy();

//TODO: non-helix case similar to StopSimulation
    // Stops simulator threads (good idea to lock around this?)
//     sim_running = false;

//    cerr << "Total simulated ins = " << dec << tstate[core0]->num_inst << endl;
//    cerr << "Total ins: " << icount.Count(0) << endl;

    if (exitCode != EXIT_SUCCESS)
        cerr << "ERROR! Exit code = " << dec << exitCode << endl;
}

/* ========================================================================== */
//Callback to collect memory addresses modified by a given instruction
//We grab the actual address before the simulator modifies it
//(assumes it is called before the actual write occurs)
VOID Zesto_WriteByteCallback(ADDRINT addr, UINT8 val_to_write)
{
    (VOID) val_to_write;

    UINT8* _addr = (UINT8*) addr;
    UINT8 val = *_addr;

    // Since map.insert doesn't change existing keys, we only capture the value
    // before the first write on that address by this inst (as we should)
    sanity_writes.insert(pair<ADDRINT, UINT8>(addr, val));
}

/* ========================================================================== */
//Checks if instruction correctly rolled back any writes it may have done.
VOID SanityMemCheck()
{
    map<ADDRINT, UINT8>::iterator it;

    UINT8* addr;
    UINT8 written_val;

    for (it = sanity_writes.begin(); it != sanity_writes.end(); it++)
    {
        addr = (UINT8*) (*it).first;
        written_val = (*it).second;

        ASSERTX(written_val == *addr);
    }
}

/* ==========================================================================
 * Called from simulator once handshake is free to be reused.
 * This allows to pipeline instrumentation and simulation. */
VOID ReleaseHandshake(UINT32 coreID)
{
    THREADID instrument_tid = core_threads[coreID];
    ASSERTX(!handshake_buffer.empty(instrument_tid));

    // pop() invalidates the buffer
    handshake_buffer.pop(instrument_tid);
}

/* ========================================================================== */
VOID SimulatorLoop(VOID* arg)
{
    THREADID instrument_tid = reinterpret_cast<THREADID>(arg);
    THREADID tid = PIN_ThreadId();

    INT32 coreID = -1;

/*    if (KnobGreedyCores.Value()) {
        INT32 host_cpus = get_nprocs_conf();
        cpu_set_t mask;
        CPU_ZERO(mask);
        CPU_SET(coreID);
        sched_setaffinity(0, sizeof (mask), &mask);
    }
*/

    while (true) {
        while (handshake_buffer.empty(instrument_tid)) {
            PIN_Yield();
	    while(consumers_sleep) {
	      PIN_Sleep(250);
	    }

            /* Check kill flag */
            thread_state_t* tstate = get_tls(instrument_tid);
            lk_lock(&tstate->lock, tid+1);

            if (!tstate->is_running || PIN_IsProcessExiting()) {
                deactivate_core(coreID);
                tstate->sim_stopped = true;
                lk_unlock(&tstate->lock);
                return;
            }
            lk_unlock(&tstate->lock);
        }

	int consumerHandshakes = handshake_buffer.getConsumerSize(instrument_tid);
        if(consumerHandshakes == 0) {
	  handshake_buffer.front(instrument_tid, false);
	  consumerHandshakes = handshake_buffer.getConsumerSize(instrument_tid);
        }
        assert(consumerHandshakes > 0);


	//        handshake_container_t* handshake = handshake_buffer.front(instrument_tid, true);
	//        ASSERTX(handshake != NULL);
	//        ASSERTX(handshake->flags.valid);

        for(int i = 0; i < consumerHandshakes; i++) {
          while(consumers_sleep) {
	      PIN_Sleep(250);
	    }
	  
	  handshake_container_t* handshake = handshake_buffer.front(instrument_tid, true);
            ASSERTX(handshake != NULL);
            ASSERTX(handshake->flags.valid);


        /* Preserving coreID if we destroy handshake before coming in here,
         * so we know which core to deactivate. */
        coreID = handshake->handshake.coreID;

#ifdef TIME_TRANSPARENCY
        // Capture time spent in simulation to ensure time syscall transparency
        UINT64 ins_delta_time = rdtsc();
#endif
        // Perform memory sanity checks for values touched by simulator
        // on previous instruction
        if (KnobSanity.Value())
            SanityMemCheck();

        thread_state_t* tstate = get_tls(instrument_tid);
        lk_lock(&tstate->lock, tid+1);
        tstate->num_inst++;
        lk_unlock(&tstate->lock);

        // Actual simulation happens here
	Zesto_Resume(&handshake->handshake, &handshake->mem_buffer, handshake->flags.isFirstInsn, handshake->flags.isLastInsn);

	if(!KnobPipelineInstrumentation.Value())
            ReleaseHandshake(handshake->handshake.coreID);

#ifdef TIME_TRANSPARENCY
        ins_delta_time = rdtsc() - ins_delta_time;
        sim_time += ins_delta_time;
#endif
	}
	lastConsumerApply[instrument_tid] = sim_cycle;
	handshake_buffer.applyConsumerChanges(instrument_tid, consumerHandshakes);
    }
}

/* ========================================================================== */
VOID MakeSSRequest(THREADID tid, ADDRINT pc, ADDRINT npc, ADDRINT tpc, BOOL brtaken, const CONTEXT *ictxt, handshake_container_t* hshake)
{
    thread_state_t* tstate = get_tls(tid);
    MakeSSContext(ictxt, &tstate->fpstate_buf, pc, npc, &hshake->handshake.ctxt);

    hshake->handshake.coreID = tstate->coreID;
    hshake->handshake.pc = pc;
    hshake->handshake.npc = npc;
    hshake->handshake.tpc = tpc;
    hshake->handshake.brtaken = brtaken;
    hshake->handshake.sleep_thread = FALSE;
    hshake->handshake.resume_thread = FALSE;
    hshake->handshake.real = TRUE;
    PIN_SafeCopy(hshake->handshake.ins, (VOID*) pc, MD_MAX_ILEN);

    hshake->handshake.slice_num = tstate->slice_num;
    hshake->handshake.feeder_slice_length = tstate->slice_length;
    hshake->handshake.slice_weight_times_1000 = tstate->slice_weight_times_1000;
}

/* ========================================================================== */
VOID GrabInstMemReads(THREADID tid, ADDRINT addr, UINT32 size, BOOL first_read, ADDRINT pc)
{
    if (handshake_buffer.numThreads() < (unsigned int) num_threads) {
        return;
    }

    if(producers_sleep) {
      PIN_Sleep(1000);
      return;
    }

    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);

    if (tstate->sleep_producer) {
      lk_unlock(&tstate->lock);
      PIN_Sleep(50);
      return;
    }
    if (tstate->ignore || tstate->ignore_all) {
        lk_unlock(&tstate->lock);
        return;
    }
    lk_unlock(&tstate->lock);

    handshake_container_t* handshake;
    if (first_read) {
        handshake = lookahead_buffer[tid].get_buffer();
	ASSERTX(!handshake->flags.valid);
    }
    else {
        handshake = lookahead_buffer[tid].get_buffer();
    }

    /* should be the common case */
    if (first_read && size <= 4) {
        handshake->handshake.mem_addr = addr;
        PIN_SafeCopy(&handshake->handshake.mem_val, (VOID*)addr, size);
        handshake->handshake.mem_size = size;
        return;
    }

    UINT8 val;
    for (UINT32 i=0; i < size; i++) {
        PIN_SafeCopy(&val, (VOID*) (addr+i), 1);
        handshake->mem_buffer.insert(pair<UINT32, UINT8>(addr + i,val));
    }
}

/* ========================================================================== */
VOID SimulateInstruction(THREADID tid, ADDRINT pc, BOOL taken, ADDRINT npc, ADDRINT tpc, const CONTEXT *ictxt, BOOL has_memory)
{
    if (handshake_buffer.numThreads() < (unsigned int) num_threads) {
        return;
    }

    if(producers_sleep) {
      PIN_Sleep(1000);
      return;
    }

    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);
    if (tstate->sleep_producer) {
      lk_unlock(&tstate->lock);
      PIN_Sleep(50);
      return;
    }
    
    if (tstate->ignore || tstate->ignore_all) {      
        lk_unlock(&tstate->lock);
        return;
    }
    lk_unlock(&tstate->lock);

    handshake_container_t* handshake;
    if (has_memory) {
      handshake = lookahead_buffer[tid].get_buffer();
    }
    else {
      handshake = lookahead_buffer[tid].get_buffer();
    }

    ASSERTX(handshake != NULL);
    ASSERTX(!handshake->flags.valid);

    // Sanity trace
    if (!KnobSanityInsTraceFile.Value().empty())
    {
        ADDRINT sanity_pc;
        sanity_trace >> sanity_pc;
#ifdef ZESTO_PIN_DBG
        if (sanity_pc != pc)
        {
            cerr << "Sanity_pc != pc. Bad things will happen!" << endl << "sanity_pc: " <<
            hex << sanity_pc << " pc: " << pc << " sim_icount: " << dec << tstate->num_inst << endl;
        }
#endif
        ASSERTX(sanity_pc == pc);
    }

    // TODO: We shouldn't have to go through this anymore. We can just grab the appropriate
    // tstate->lock! Fix me!
    /* In case tls was not updated yet (because we can't touch it
     * from other threads), look up in reverse mapping */
    if ((tstate->coreID == (ADDRINT)-1))
    {
        map<UINT32,THREADID>::iterator it;
        for (it = core_threads.begin(); it != core_threads.end(); it++)
            if ((*it).second == tid)
            {
                tstate->coreID = (*it).first;
                break;
            }
        ASSERTX(tstate->coreID != (ADDRINT)-1);
    }

    tstate->queue_pc(pc);

    if (handshake->flags.isFirstInsn)
    {
        Zesto_SetBOS(tstate->coreID, tstate->bos);
        lk_lock(&tstate->lock, tid+1);
        tstate->sim_stopped = false;
        lk_unlock(&tstate->lock);
    }

    // Populate handshake buffer
    MakeSSRequest(tid, pc, npc, tpc, taken, ictxt, handshake);

    // Clear memory sanity check buffer - callbacks should fill it in SimulatorLoop
    if (KnobSanity.Value())
        sanity_writes.clear();

    // Let simulator consume instruction from SimulatorLoop
    handshake->flags.valid = true;

    lookahead_buffer[tid].push_done();
    if(lookahead_buffer[tid].full()) {
      flushOneToHandshakeBuffer(tid);
    }
}

/* ========================================================================== */
//Trivial call to let us do conditional instrumentation based on an argument
ADDRINT returnArg(BOOL arg)
{
   return arg;
}

VOID WarmCacheRead(VOID * addr)
{
    Zesto_WarmLLC((ADDRINT)addr, false);
}

VOID WarmCacheWrite(VOID * addr)
{
    Zesto_WarmLLC((ADDRINT)addr, true);
}

/* ========================================================================== */
VOID Instrument(INS ins, VOID *v)
{
    // ILDJIT is doing its initialization/compilation/...
    if (KnobILDJIT.Value() && !ILDJIT_IsExecuting())
        return;
    
    // Tracing
    if (!KnobInsTraceFile.Value().empty()) {
             ADDRINT pc = INS_Address(ins);
             USIZE size = INS_Size(ins);

             trace_file << pc << " " << INS_Disassemble(ins);
	     pc_diss[pc] = string(INS_Disassemble(ins));
             for (INT32 curr = size-1; curr >= 0; curr--)
                trace_file << " " << int(*(UINT8*)(curr + pc));
             trace_file << endl;
    }


    // Not executing yet, only warm caches, if needed
    if (ExecMode != EXECUTION_MODE_SIMULATE)
    {
        if (KnobWarmLLC.Value())
        {
            UINT32 memOperands = INS_MemoryOperandCount(ins);

            // Iterate over each memory operand of the instruction.
            for (UINT32 memOp = 0; memOp < memOperands; memOp++)
            {
                if (INS_MemoryOperandIsRead(ins, memOp))
                {
                    INS_InsertPredicatedCall(
                        ins, IPOINT_BEFORE, (AFUNPTR)WarmCacheRead,
                        IARG_MEMORYOP_EA, memOp,
                        IARG_END);
                }
                if (INS_MemoryOperandIsWritten(ins, memOp))
                {
                    INS_InsertPredicatedCall(
                        ins, IPOINT_BEFORE, (AFUNPTR)WarmCacheWrite,
                        IARG_MEMORYOP_EA, memOp,
                        IARG_END);
                }
            }
        }
        return;
    }

    UINT32 memOperands = INS_MemoryOperandCount(ins);
    UINT32 memReads = 0;
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
        if (INS_MemoryOperandIsRead(ins, memOp))
        {
            UINT32 memSize = INS_MemoryOperandSize(ins, memOp);
            memReads++;
            if(INS_HasRealRep(ins))
            {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
                INS_InsertThenCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)GrabInstMemReads,
                    IARG_THREAD_ID,
                    IARG_MEMORYOP_EA, memOp,
                    IARG_UINT32, memSize,
                    IARG_BOOL, (memReads == 1),
                    IARG_INST_PTR,
                    IARG_END);
            }
            else
            {
                INS_InsertCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)GrabInstMemReads,
                    IARG_THREAD_ID,
                    IARG_MEMORYOP_EA, memOp,
                    IARG_UINT32, memSize,
                    IARG_BOOL, (memReads == 1),
                    IARG_INST_PTR,
                    IARG_END);
            }
        }
    }


    if (! INS_IsBranchOrCall(ins))
    {
        // REP-ed instruction: only instrument first iteration
        // (simulator will return once all iterations are done)
        if(INS_HasRealRep(ins))
        {
           INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
           INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR) SimulateInstruction,
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_BOOL, 0,
                       IARG_ADDRINT, INS_NextAddress(ins),
                       IARG_FALLTHROUGH_ADDR,
                       IARG_CONST_CONTEXT,
                       IARG_BOOL, (memReads > 0),
                       IARG_END);
        }
        else
            // Non-REP-ed, non-branch instruction, use falltrough
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) SimulateInstruction,
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_BOOL, 0,
                       IARG_ADDRINT, INS_NextAddress(ins),
                       IARG_FALLTHROUGH_ADDR,
                       IARG_CONST_CONTEXT,
                       IARG_BOOL, (memReads > 0),
                       IARG_END);
    }
    else
    {
        // Branch, give instrumentation appropriate address
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) SimulateInstruction,
                   IARG_THREAD_ID,
                   IARG_INST_PTR,
                   IARG_BRANCH_TAKEN,
                   IARG_ADDRINT, INS_NextAddress(ins),
                   IARG_BRANCH_TARGET_ADDR,
                   IARG_CONST_CONTEXT,
                   IARG_BOOL, (memReads > 0),
                   IARG_END);
    }
}

/* ========================================================================== */
/** The command line arguments passed upon invocation need paring because (1) the
 * command line can have arguments for SimpleScalar and (2) Pin cannot see the
 * SimpleScalar's arguments otherwise it will barf; it'll expect KNOB
 * declarations for those arguments. Thereforce, we follow a convention that
 * anything declared past "-s" and before "--" on the command line must be
 * passed along as SimpleScalar's argument list.
 *
 * SimpleScalar's arguments are extracted out of the command line in two steps:
 * First, we create a new argument vector that can be passed to SimpleScalar.
 * This is done by calloc'ing and copying the arguments over. Thereafter, in the
 * second stage we nullify SimpleScalar's arguments from the original (Pin's)
 * command line so that Pin doesn't see during its own command line parsing
 * stage. */
SSARGS MakeSimpleScalarArgcArgv(UINT32 argc, CHAR *argv[])
{
    CHAR   **ssArgv   = 0;
    UINT32 ssArgBegin = 0;
    UINT32 ssArgc     = 0;
    UINT32 ssArgEnd   = argc;

    for (UINT32 i = 0; i < argc; i++)
    {
        if ((string(argv[i]) == "-s") || (string(argv[i]) == "--"))
        {
            ssArgBegin = i + 1;             // Points to a valid arg
            break;
        }
    }

    if (ssArgBegin)
    {
        ssArgc = (ssArgEnd - ssArgBegin)    // Specified command line args
                 + (1);                     // Null terminator for argv
    }
    else
    {
        // Coming here implies the command line has not been setup properly even
        // to run Pin, so return. Pin will complain appropriately.
        return make_pair(argc, argv);
    }

    // This buffer will hold SimpleScalar's argv
    ssArgv = reinterpret_cast <CHAR **> (calloc(ssArgc, sizeof(CHAR *)));

    UINT32 ssArgIndex = 0;
    ssArgv[ssArgIndex++] = sim_name;  // Does not matter; just for sanity
    for (UINT32 pin = ssArgBegin; pin < ssArgEnd; pin++)
    {
        if (string(argv[pin]) != "--")
        {
            string *argvCopy = new string(argv[pin]);
            ssArgv[ssArgIndex++] = const_cast <CHAR *> (argvCopy->c_str());
        }
    }

    // Terminate the argv. Ending must terminate with a pointer *referencing* a
    // NULL. Simply terminating the end of argv[n] w/ NULL violates conventions
    ssArgv[ssArgIndex++] = new CHAR('\0');

    return make_pair(ssArgc, ssArgv);
}


/* ========================================================================== */
/* Returns true if thread was scheduled to run */
/*static BOOL RemoveRunQueueThread(THREADID tid)
{
    list<THREADID>::iterator it;
    for (it = run_queue.begin(); it != run_queue.end(); it++)
        if (*it == tid) {
            run_queue.erase(it);
            return true;
        }
    return false;
}*/

/* ========================================================================== */
VOID ScheduleRunQueue()
{
    cerr << "RQ size: " << run_queue.size() << endl;

    // XXX: Conservative for now -- assume nThreads == nCores
    ASSERTX(run_queue.size() == (unsigned int)num_threads);

    list<THREADID>::iterator it = run_queue.begin();
    INT32 nextCoreID;
    for (nextCoreID = num_threads-1; nextCoreID >= 0; nextCoreID--, it++) {
        core_threads[nextCoreID] = *it;
        thread_cores[*it] = nextCoreID;
        cerr << "Core: " << nextCoreID << " " << *it << endl;
    }

    run_queue.clear();
}

/* ========================================================================== */
VOID ThreadStart(THREADID threadIndex, CONTEXT * ictxt, INT32 flags, VOID *v)
{
    // ILDJIT is forking a compiler thread, ignore
//    if (KnobILDJIT.Value() && !ILDJIT_IsCreatingExecutor())
//        return;

    lk_lock(&test, 1);

    thread_state_t* tstate = new thread_state_t(threadIndex);
    if (control.PinPointsActive())
        tstate->slice_num = 1;      // PP slices aren't 0-indexed
    PIN_SetThreadData(tls_key, tstate, threadIndex);

    ADDRINT tos, bos;

    tos = PIN_GetContextReg(ictxt, REG_ESP);
    CHAR** sp = (CHAR**)tos;
//    cerr << hex << "SP: " << (VOID*) sp << dec << endl;

    // We care about the address space only on main thread creation
    if (threadIndex == 0) {
        UINT32 argc = *(UINT32*) sp;
//        cerr << hex << "argc: " << argc << dec << endl;

        for(UINT32 i=0; i<argc; i++) {
            sp++;
//            cerr << hex << (ADDRINT)(*sp) << dec << endl;
        }
        CHAR* last_argv = *sp;
        sp++;   // End of argv (=NULL);

        sp++;   // Start of envp

        CHAR** envp = sp;
//        cerr << "envp: " << hex << (ADDRINT)envp << endl;
        while(*envp != NULL) {
//            cerr << hex << (ADDRINT)(*envp) << dec << endl;
            envp++;
        } // End of envp

        CHAR* last_env = *(envp-1);
        envp++; // Skip end of envp (=NULL)

        Elf32_auxv_t* auxv = (Elf32_auxv_t*)envp;
//        cerr << "auxv: " << hex << auxv << endl;
        for (; auxv->a_type != AT_NULL; auxv++) { //go to end of aux_vector
            // This containts the address of the kernel-mapped page used for a fast syscall routine
            if (auxv->a_type == AT_SYSINFO) {
#ifdef ZESTO_PIN_DBG
                cerr << "AT_SYSINFO: " << hex << auxv->a_un.a_val << endl;
#endif
                ADDRINT vsyscall_page = (ADDRINT)(auxv->a_un.a_val & 0xfffff000);
                ASSERTX( Zesto_Notify_Mmap(0/*coreID*/, vsyscall_page, MD_PAGE_SIZE, false) );
            }
        }

        if (last_env != NULL)
            bos = (ADDRINT) last_env + strlen(last_env)+1;
        else
            bos = (ADDRINT) last_argv + strlen(last_argv)+1; //last_argv != NULLalways
//        cerr << "bos: " << hex << bos << dec << endl;

        // Reserve space for environment and arguments in case
        // execution starts on another thread.
        ADDRINT tos_start = ROUND_DOWN(tos, MD_PAGE_SIZE);
        ADDRINT bos_end = ROUND_UP(bos, MD_PAGE_SIZE);
        ASSERTX( Zesto_Notify_Mmap(0/*coreID*/, tos_start, bos_end-tos_start, false));

    }
    else {
        bos = tos;
    }

    tstate->bos = bos;

    // Application threads only -- create buffers for them
    // and make sure a respective simulator thread gets created
    if (!KnobILDJIT.Value() ||
        (KnobILDJIT.Value() && ILDJIT_IsCreatingExecutor()))
    {
        // Create new buffer to store thread context
        PIN_RWMutexWriteLock(&simbuffer_lock);

        handshake_buffer.allocateThread(threadIndex);

        thread_list.push_back(threadIndex);

        run_queue.push_back(threadIndex);

        PIN_RWMutexUnlock(&simbuffer_lock);

        // Will get cleared on first simulated instruction
        tstate->sim_stopped = true;

        // This will trigger spawning a sim thread
        lk_lock(&instrument_tid_lock, threadIndex+1);
        instrument_tid_queue.push(threadIndex);
        lk_unlock(&instrument_tid_lock);
    }

    /*    cpu_set_t mask;
    CPU_ZERO(&mask);
    for (int i=0; i<8; i++)
      CPU_SET(i, &mask);
      sched_setaffinity(0, sizeof(cpu_set_t), &mask);*/
    lk_unlock(&test);
}


UINT8 syscall_template[] = {0xcd, 0x80};

/* ========================================================================== */
VOID PauseSimulation(THREADID tid)
{
    /* The context is that all cores functionally have sent signal 0
     * and unblocked the last iteration. We need to (i) wait for them
     * to functionally reach wait 0, where they will wait until the end
     * of the loop; (ii) drain all pipelines once cores are waiting. */
  cerr << "pausing..." << endl;
  flushLookahead(tid, 0);
  
  volatile bool done_with_iteration = false;
    do {
        done_with_iteration = true;
        vector<THREADID>::iterator it;
        for (it = thread_list.begin(); it != thread_list.end(); it++) {
            if ((*it) != tid) {
                thread_state_t* tstate = get_tls(*it);
                lk_lock(&tstate->lock, tid + 1);
                bool curr_done = tstate->ignore && (tstate->lastWaitID == 0);
		if(curr_done) {
		  assert(lookahead_buffer[*it].size() == 0);
		  assert(lookahead_buffer[*it].empty());
		}
                done_with_iteration &= curr_done;
                /* Setting ignore_all here (while ignore is set) should be a race-free way
                 * of ignoring the serial portion outside the loop after the thread goes
                 * on an unsets ignore locally. */
                if (curr_done) {
                    tstate->ignore_all = true;
		    tstate->sleep_producer = true;
		}
                lk_unlock(&tstate->lock);
            }
        }
    } while (!done_with_iteration);

    /* Here we have produced everything for this loop! */
    enable_consumers();

    /* Drainning all pipelines and deactivating cores. */
    vector<THREADID>::iterator it;
    for (it = thread_list.begin(); it != thread_list.end(); it++) {
        INT32 coreID = thread_cores[*it];

        /* Insert a trap. This will ensure that the pipe drains before
         * consuming the next instruction.*/
        handshake_container_t* handshake = handshake_buffer.get_buffer(*it);
        handshake->flags.isFirstInsn = false;
        handshake->handshake.sleep_thread = false;
        handshake->handshake.resume_thread = false;
        handshake->handshake.real = false;
        handshake->handshake.coreID = coreID;
        handshake->flags.valid = true;

        handshake->handshake.pc = (ADDRINT) syscall_template;
        handshake->handshake.npc = (ADDRINT) syscall_template + sizeof(syscall_template);
        handshake->handshake.tpc = (ADDRINT) syscall_template + sizeof(syscall_template);
        handshake->handshake.brtaken = false;
        memcpy(handshake->handshake.ins, syscall_template, sizeof(syscall_template));
        handshake_buffer.producer_done(*it, true);

        /* Deactivate this core, so we can advance the cycle conunter of
         * others without waiting on it */
        handshake_container_t* handshake_2 = handshake_buffer.get_buffer(*it);

        handshake_2->flags.isFirstInsn = false;
        handshake_2->handshake.sleep_thread = true;
        handshake_2->handshake.resume_thread = false;
        handshake_2->handshake.real = false;
        handshake_2->handshake.pc = 0;
        handshake_2->handshake.coreID = coreID;
        handshake_2->flags.valid = true;
        handshake_buffer.producer_done(*it, true);

        /* And finally, flush the core's pipelie to get rid of anything
         * left over (including the trap) and flush the ring cache */
        handshake_container_t* handshake_3 = handshake_buffer.get_buffer(*it);

        handshake_3->flags.isFirstInsn = false;
        handshake_3->handshake.sleep_thread = false;
        handshake_3->handshake.resume_thread = false;
        handshake_3->handshake.flush_pipe = true;
        handshake_3->handshake.real = false;
        handshake_3->handshake.pc = 0;
        handshake_3->handshake.coreID = coreID;
        handshake_3->flags.valid = true;
        handshake_buffer.producer_done(*it, true);

        handshake_buffer.flushBuffers(*it);
    }

    enable_consumers();

    /* Wait until all cores are done -- consumed their buffers. */
    volatile bool done = false;
    do {
        done = true;
        vector<THREADID>::iterator it;
        for (it = thread_list.begin(); it != thread_list.end(); it++) {
            done &= handshake_buffer.empty((*it));
        }
	// vvv DO THE SAME FOR PRODUCER_SLEEP IN SIMULATEINSTUCTION, etc
       //      if (!done)  MAKE THIS SENSITIVE TO THE NUMBER OF REMAINING HANDSHAKES!!!!!
	//        if (!done) 
	//	  PIN_Sleep(100);
	  //            PIN_Yield();
    } while (!done);

#ifdef ZESTO_PIN_DBG
    cerr << tid << " [" << sim_cycle << ":KEVIN]: All cores have empty buffers" << endl;
    cerr.flush();
#endif

    for (it = thread_list.begin(); it != thread_list.end(); it++) {	
      cerr << thread_cores[*it] << ":OverlapCycles:" << sim_cycle - lastConsumerApply[*it] << endl;
    }    

    /* Have thread ignore serial section after */
    /* XXX: Do we need this? A few lines above we set ignore_all! */
    for (it = thread_list.begin(); it != thread_list.end(); it++) {
        thread_state_t* tstate = get_tls(*it);
        lk_lock(&tstate->lock, *it+1);
        handshake_buffer.resetPool(*it);
        tstate->ignore = true;
	tstate->sleep_producer = false;
        lk_unlock(&tstate->lock);
	assert(lookahead_buffer[*it].empty());
    }
}

/* ========================================================================== */
VOID ResumeSimulation(THREADID tid)
{

    /* All cores were sleeping in between loops, wake them up now. */
    vector<THREADID>::iterator it;
    for (it = thread_list.begin(); it != thread_list.end(); it++) {
        INT32 coreID = thread_cores[*it];

        /* Wake up cores right away without going through the handshake
         * buffer (which should be empty anyway).
         * If we do go through it, there are no guarantees for when the
         * resume is consumed, which can lead to nasty races of who gets
         * to resume first. */
        ASSERTX(handshake_buffer.empty(*it));
        ASSERTX(lookahead_buffer[*it].empty());
        activate_core(coreID);
        thread_state_t* tstate = get_tls(*it);
        lk_lock(&tstate->lock, tid+1);
        tstate->ignore_all = false;
        lk_unlock(&tstate->lock);
    }
}


/* ========================================================================== */
VOID StopSimulation(THREADID tid)
{
    /* Deactivate this core, so simulation doesn't wait on it */
    thread_state_t* tstate = get_tls(tid);
    deactivate_core(tstate->coreID);

    /* Signal simulator threads to die */
    vector<THREADID>::iterator it;
    for(it = thread_list.begin(); it != thread_list.end(); it++) {
        thread_state_t* curr_tstate = get_tls(*it);
        lk_lock(&curr_tstate->lock, tid+1);
        curr_tstate->is_running = false;
        lk_unlock(&curr_tstate->lock);
    }

    /* Spin until SimulatorLoop actually finishes */
    volatile bool is_stopped;
    UINT64 total_inst;
    do {
        is_stopped = true;
        total_inst = 0;

        for(it = thread_list.begin(); it != thread_list.end(); it++) {
            thread_state_t* curr_tstate = get_tls(*it);
            lk_lock(&curr_tstate->lock, tid+1);
            is_stopped &= curr_tstate->sim_stopped;
            total_inst += curr_tstate->num_inst;
            lk_unlock(&curr_tstate->lock);
        }
    } while(!is_stopped);

    Zesto_Slice_End(0, 0, total_inst, 100000);

    /* Reaching this ensures SimulatorLoop is not in the middle
     * of simulating an instruction. We can safely blow away
     * the pipeline and end the simulation. */
    Fini(EXIT_SUCCESS, NULL);
    PIN_ExitProcess(EXIT_SUCCESS);
}

/* ========================================================================== */
VOID ThreadFini(THREADID threadIndex, const CONTEXT *ctxt, INT32 code, VOID *v)
{
    cerr << "Thread exit. ID: " << threadIndex << endl;

    lk_lock(&test, 1);
    thread_state_t* tstate = get_tls(threadIndex);

    /* No need to schedule this thread any more */
    //RemoveRunQueueThread(threadIndex);

    BOOL was_scheduled = handshake_buffer.hasThread(threadIndex);

    /* Ignore threads which we weren't going to simulate */
    if (!was_scheduled) {
        delete tstate;
        lk_unlock(&test);
        return;
    }

    /* There will be no further instructions instrumented (on this thread).
     * Make sure to kill the simulator thread (if any). */
// XXX: Rethink when we are not tied to HELIX
#if 0
    lk_lock(&simbuffer_lock, threadIndex+1);
    handshake_container_t *handshake = handshake_buffer.front(threadIndex);
    INT32 coreID = -1;
    if (handshake) {
        coreID = handshake->handshake.coreID;
        handshake->flags.killThread = true;
    }
    lk_unlock(&simbuffer_lock);
#endif
    if (!run_queue.empty()) {
        cerr << "NYI: Thread rescheduling" << endl;
        PIN_ExitProcess(1);
    }
    else {
// Rethink when we are not tied to HELIX
#if 0
        lk_lock(&simbuffer_lock, threadIndex+1);

        if (sim_running) {
            sim_running = false;

            lk_unlock(&simbuffer_lock);

            vector<THREADID>::iterator it;
            /* Spin until SimulatorLoop actually finishes */
            volatile bool is_stopped;
            do {
                lk_lock(&simbuffer_lock, threadIndex+1);
                is_stopped = true;
                for(it = thread_list.begin(); it != thread_list.end(); it++) {
                    is_stopped &= sim_stopped[(*it)];
                    cerr << (*it) << " " << sim_stopped[(*it)] << " ";
                }
                cerr << endl;
                lk_unlock(&simbuffer_lock);
            } while(!is_stopped);

            /* Reaching this ensures SimulatorLoop is not in the middle
             * of simulating an instruction. We can safely blow away
             * the pipeline and end the simulation. */
            if (coreID != -1)
                Zesto_Slice_End(coreID, 0, SimOrgInsCount, 100000);
            Fini(EXIT_SUCCESS, NULL);
            PIN_ExitProcess(EXIT_SUCCESS);
        }
        else
            lk_unlock(&simbuffer_lock);
#endif
    }

    delete tstate;
    lk_unlock(&test);
}

//from linux/arch/x86/ia32/sys_ia32.c
struct mmap_arg_struct {
     UINT32 addr;
     UINT32 len;
     UINT32 prot;
     UINT32 flags;
     UINT32 fd;
     UINT32 offset;
};

//from times.h
struct tms {
    clock_t tms_utime;
    clock_t tms_stime;
    clock_t tms_cutime;
    clock_t tms_cstime;
};

/* ========================================================================== */
VOID SyscallEntry(THREADID threadIndex, CONTEXT * ictxt, SYSCALL_STANDARD std, VOID *v)
{
    // ILDJIT is minding its own bussiness
//    if (KnobILDJIT.Value() && !ILDJIT_IsExecuting())
//        return;

    lk_lock(&test, threadIndex+1);

    ADDRINT syscall_num = PIN_GetSyscallNumber(ictxt, std);
    ADDRINT arg1 = PIN_GetSyscallArgument(ictxt, std, 0);
    ADDRINT arg2;
    ADDRINT arg3;
    mmap_arg_struct mmap_arg;

    thread_state_t* tstate = get_tls(threadIndex);

    tstate->last_syscall_number = syscall_num;

    switch(syscall_num)
    {
      case __NR_brk:
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall brk(" << dec << syscall_num << ") addr: 0x" << hex << arg1 << dec << endl;
#endif
        tstate->last_syscall_arg1 = arg1;
        break;

      case __NR_munmap:
        arg2 = PIN_GetSyscallArgument(ictxt, std, 1);
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall munmap(" << dec << syscall_num << ") addr: 0x" << hex << arg1
             << " length: " << arg2 << dec << endl;
#endif
        tstate->last_syscall_arg1 = arg1;
        tstate->last_syscall_arg2 = arg2;
        break;

      case __NR_mmap: //oldmmap
        memcpy(&mmap_arg, (void*)arg1, sizeof(mmap_arg_struct));
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall oldmmap(" << dec << syscall_num << ") addr: 0x" << hex << mmap_arg.addr
             << " length: " << mmap_arg.len << dec << endl;
#endif
        tstate->last_syscall_arg1 = mmap_arg.len;
        break;

      case __NR_mmap2:
        arg2 = PIN_GetSyscallArgument(ictxt, std, 1);
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall mmap2(" << dec << syscall_num << ") addr: 0x" << hex << arg1
             << " length: " << arg2 << dec << endl;
#endif
        tstate->last_syscall_arg1 = arg2;
        break;

      case __NR_mremap:
        arg2 = PIN_GetSyscallArgument(ictxt, std, 1);
        arg3 = PIN_GetSyscallArgument(ictxt, std, 2);
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall mremap(" << dec << syscall_num << ") old_addr: 0x" << hex << arg1
             << " old_length: " << arg2 << " new_length: " << arg3 << dec << endl;
#endif
        tstate->last_syscall_arg1 = arg1;
        tstate->last_syscall_arg2 = arg2;
        tstate->last_syscall_arg3 = arg3;
        break;

#ifdef TIME_TRANSPARENCY
      case __NR_times:
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall times(" << dec << syscall_num << ") num_ins: " << SimOrgInsCount << endl;
#endif
        tstate->last_syscall_arg1 = arg1;
        break;
#endif
      case __NR_mprotect:
        arg2 = PIN_GetSyscallArgument(ictxt, std, 1);
        arg3 = PIN_GetSyscallArgument(ictxt, std, 2);
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall mprotect(" << dec << syscall_num << ") addr: " << hex << arg1
             << dec << " length: " << arg2 << " prot: " << hex << arg3 << dec << endl;
#endif
        tstate->last_syscall_arg1 = arg1;
        tstate->last_syscall_arg2 = arg2;
        tstate->last_syscall_arg3 = arg3;
        break;

#ifdef ZESTO_PIN_DBG
    case __NR_open:
        cerr << "Syscall open (" << dec << syscall_num << ") path: " << (char*)arg1 << endl;
        break;
#endif

#ifdef ZESTO_PIN_DBG
    case __NR_exit:
        cerr << "Syscall exit (" << dec << syscall_num << ") code: " << arg1 << endl;
        break;
#endif

/*
    case __NR_sysconf:
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall sysconf (" << dec << syscall_num << ") arg: " << arg1 << endl;
#endif
        tstate->last_syscall_arg1 = arg1;
        break;
*/
      default:
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall " << dec << syscall_num << endl;
#endif
        break;
    }
    lk_unlock(&test);
}

/* ========================================================================== */
VOID SyscallExit(THREADID threadIndex, CONTEXT * ictxt, SYSCALL_STANDARD std, VOID *v)
{
    // ILDJIT is minding its own bussiness
//    if (KnobILDJIT.Value() && !ILDJIT_IsExecuting())
//        return;

    lk_lock(&test, threadIndex+1);
    ADDRINT retval = PIN_GetSyscallReturn(ictxt, std);

    thread_state_t* tstate = get_tls(threadIndex);

#ifdef TIME_TRANSPARENCY
    //for times()
    tms* buf;
    clock_t adj_time;
#endif

    switch(tstate->last_syscall_number)
    {
      case __NR_brk:
#ifdef ZESTO_PIN_DBG
        cerr << "Ret syscall brk(" << dec << tstate->last_syscall_number << ") addr: 0x"
             << hex << retval << dec << endl;
#endif
        if(tstate->last_syscall_arg1 != 0)
            Zesto_UpdateBrk(0/*coreID*/, tstate->last_syscall_arg1, true);
        /* Seemingly libc code calls sbrk(0) to get the initial value of the sbrk. We intercept that and send result to zesto, so that we can correclty deal with virtual memory. */
        else
            Zesto_UpdateBrk(0/*coreID*/, retval, false);
        break;

      case __NR_munmap:
#ifdef ZESTO_PIN_DBG
        cerr << "Ret syscall munmap(" << dec << tstate->last_syscall_number << ") addr: 0x"
             << hex << tstate->last_syscall_arg1 << " length: " << tstate->last_syscall_arg2 << dec << endl;
#endif
        if(retval != (ADDRINT)-1)
            Zesto_Notify_Munmap(0/*coreID*/, tstate->last_syscall_arg1, tstate->last_syscall_arg2, false);
        break;

      case __NR_mmap: //oldmap
#ifdef ZESTO_PIN_DBG
        cerr << "Ret syscall oldmmap(" << dec << tstate->last_syscall_number << ") addr: 0x"
             << hex << retval << " length: " << tstate->last_syscall_arg1 << dec << endl;
#endif
        if(retval != (ADDRINT)-1)
            ASSERTX( Zesto_Notify_Mmap(0/*coreID*/, retval, tstate->last_syscall_arg1, false) );
        break;

      case __NR_mmap2:
#ifdef ZESTO_PIN_DBG
        cerr << "Ret syscall mmap2(" << dec << tstate->last_syscall_number << ") addr: 0x"
             << hex << retval << " length: " << tstate->last_syscall_arg1 << dec << endl;
#endif
        if(retval != (ADDRINT)-1)
            ASSERTX( Zesto_Notify_Mmap(0/*coreID*/, retval, tstate->last_syscall_arg1, false) );
        break;

      case __NR_mremap:
#ifdef ZESTO_PIN_DBG
        cerr << "Ret syscall mremap(" << dec << tstate->last_syscall_number << ") " << hex
             << " old_addr: 0x" << tstate->last_syscall_arg1
             << " old_length: " << tstate->last_syscall_arg2
             << " new address: 0x" << retval
             << " new_length: " << tstate->last_syscall_arg3 << dec << endl;
#endif
        if(retval != (ADDRINT)-1)
        {
            ASSERTX( Zesto_Notify_Munmap(0/*coreID*/, tstate->last_syscall_arg1, tstate->last_syscall_arg2, false) );
            ASSERTX( Zesto_Notify_Mmap(0/*coreID*/, retval, tstate->last_syscall_arg3, false) );
        }
        break;

      case __NR_mprotect:
        if(retval != (ADDRINT)-1)
        {
            if ((tstate->last_syscall_arg3 & PROT_READ) == 0)
                ASSERTX( Zesto_Notify_Munmap(0/*coreID*/, tstate->last_syscall_arg1, tstate->last_syscall_arg2, false) );
            else
                ASSERTX( Zesto_Notify_Mmap(0/*coreID*/, tstate->last_syscall_arg1, tstate->last_syscall_arg2, false) );
        }
        break;

    /* Present ourself as if we have num_threads cores */
/*    case __NR_sysconf:
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall sysconf (" << dec << syscall_num << ") ret" << endl;
#endif
        if (tstate->last_syscall_arg1 == _SC_NPROCESSORS_ONLN)
            if ((INT32)retval != - 1) {
                PIN_SetContextReg(ictxt, REG_EAX, num_threads);
                PIN_ExecuteAt(ictxt);
            }
        break;*/

#ifdef TIME_TRANSPARENCY
      case __NR_times:
        buf = (tms*) tstate->last_syscall_arg1;
        adj_time = retval - (clock_t) sim_time;
#ifdef ZESTO_PIN_DBG
        cerr << "Ret syscall times(" << dec << tstate->last_syscall_number << ") old: "
             << retval  << " adjusted: " << adj_time
             << " user: " << buf->tms_utime
             << " user_adj: " << (buf->tms_utime - sim_time)
             << " system: " << buf->tms_stime << endl;
#endif
        /* Compensate for time we spent on simulation
         * Included for full transparency - some apps detect we are taking a long time
         * and do bad things like dropping frames
         * Since we have no decent way of measuring how much time the simulator spends in the OS
         * (other than calling times() for every instruction), we assume the simulator is ainly
          user code. XXX: how reasonable is this assmuption???
         */
        buf->tms_utime -= sim_time;
        /* buf->tms_stime -=  0.1 * sim_time; ?? */
        // Don't touch child process timing -- we don't support child processes anyway

        // Adjust aggregate time passed by time spent in sim
        // Return value as 32-bit int in EAX
        if ((INT32)retval != - 1)
            PIN_SetContextReg(ictxt, REG_EAX, adj_time);
        //XXX: To make this work, we need to use PIN_ExecuteAt()
        break;
#endif

      default:
        break;
    }
    lk_unlock(&test);
}

/* ========================================================================== */
VOID SimulatorThreadSpawner(VOID* arg)
{
    (void) arg;

    while (true)
    {
        // Is it time to die?
        if (PIN_IsProcessExiting()) {
            break;
        }

        // Check if there's an insturment thread without the appropriate simulator thread
        // and spawn one, if necessary
        lk_lock(&instrument_tid_lock, 1);
        if (!instrument_tid_queue.empty())
        {
            THREADID instrument_tid = instrument_tid_queue.front();
            instrument_tid_queue.pop();
            PIN_SpawnInternalThread(SimulatorLoop, reinterpret_cast<VOID*>(instrument_tid),
                                    0, NULL);
        }
        lk_unlock(&instrument_tid_lock);

        // Go to sleep -- no need to constatnly check
        PIN_Sleep(1000);
    }
}

/* ========================================================================== */
INT32 main(INT32 argc, CHAR **argv)
{
#ifdef ZESTO_PIN_DBG
    cerr << "Command line: ";
    for(int i=0; i<argc; i++)
       cerr << argv[i] << " ";
    cerr << endl;
#endif

    SSARGS ssargs = MakeSimpleScalarArgcArgv(argc, argv);

    lk_init(&test);
    PIN_RWMutexInit(&simbuffer_lock);
    lk_init(&instrument_tid_lock);

    // Obtain  a key for TLS storage.
    tls_key = PIN_CreateThreadDataKey(0);

    PIN_Init(argc, argv);
    PIN_InitSymbols();


    if(KnobAMDHack.Value()) {
      amd_hack();
    }

    if (KnobILDJIT.Value())
        MOLECOOL_Init();

    if (!KnobILDJIT.Value()) {
        // Try activate pinpoints alarm, must be done before PIN_StartProgram
        if(control.CheckKnobs(PPointHandler, 0) != 1) {
            cerr << "Error reading control parametrs, exiting." << endl;
            return 1;
        }
    }

    icount.Activate();

    if(!KnobInsTraceFile.Value().empty())
    {
        trace_file.open(KnobInsTraceFile.Value().c_str());	
        trace_file << hex;
	pc_file.open("pcs.trace");
	pc_file << hex;
    }

    if(!KnobSanityInsTraceFile.Value().empty())
    {
        sanity_trace.open(KnobSanityInsTraceFile.Value().c_str(), ifstream::in);
        if(sanity_trace.fail())
        {
            cerr << "Couldn't open sanity trace file " << KnobSanityInsTraceFile.Value() << endl;
            return 1;
        }
        sanity_trace >> hex;
    }

    // Delay this instrumentation until startSimulation call in ILDJIT.
    // This cuts down HELIX compilation noticably for integer benchmarks.

    if(!KnobILDJIT.Value()) {
      INS_AddInstrumentFunction(Instrument, 0);
    }

    PIN_AddThreadStartFunction(ThreadStart, NULL);
    PIN_AddThreadFiniFunction(ThreadFini, NULL);
    IMG_AddUnloadFunction(ImageUnload, 0);
    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_AddSyscallEntryFunction(SyscallEntry, 0);
    PIN_AddSyscallExitFunction(SyscallExit, 0);
    PIN_AddFiniFunction(Fini, 0);

    if(KnobSanity.Value())
        Zesto_Add_WriteByteCallback(Zesto_WriteByteCallback);
    sim_release_handshake = KnobPipelineInstrumentation.Value();

    Zesto_SlaveInit(ssargs.first, ssargs.second);

    host_cpus = get_nprocs_conf();

    enable_producers();
    disable_consumers();

    // The only safe way to spawn internal pin threads is from main()
    // or other internal threads, so we create a thread spawner here
    PIN_SpawnInternalThread(SimulatorThreadSpawner, NULL, 0, NULL);

    PIN_StartProgram();

    return 0;
}

void spawn_new_thread(void entry_point(void*), void* arg)
{
    PIN_SpawnInternalThread(entry_point, arg, 0, NULL);
}

VOID amd_hack()
{
    // use kernel version to distinguish between RHEL5 and RHEL6
    bool rhel6 = false;

    ifstream procversion("/proc/version");
    string version((istreambuf_iterator<char>(procversion)), istreambuf_iterator<char>());

    if (version.find(".el6.") != string::npos) {
      rhel6 = true;
    }
    else if (version.find(".el5 ") == string::npos) {
      if (version.find(".el5.") == string::npos) {
	if(version.find(".el5_") == string::npos) {
	  cerr << "ERROR! Neither .el5 nor .el6 occurs in /proc/version" << endl;
	  abort();
	}
      }
    }

    // under RHEL6, the VDSO page location can vary from build to build
    unsigned long vdso_begin, vdso_end = 0;

    if (rhel6) {
      ifstream maps("/proc/self/maps");
      string line;

      while (getline(maps, line))
        if (line.find("[vdso]") != string::npos) {
          istringstream linestream(line);
          linestream >> hex;

          if (linestream >> vdso_begin &&
              linestream.get() == '-'  &&
              linestream >> vdso_end)
            break;
          cerr << "ERROR! Badly formatted [vdso] map line: " << line << endl;
          abort();
        }

      if (vdso_end == 0) {
        cerr << "ERROR! No VDSO page map in /proc/self/maps" << endl;
        abort();
      } else if (vdso_end - vdso_begin != 0x1000) {
        cerr << "ERROR! VDSO page size isn't 0x1000 in /proc/self/maps" << endl;
        abort();
      }
    } else {
      vdso_begin = 0xffffe000;
    }

    int returnval = mprotect((void *)vdso_begin, 0x1000, PROT_EXEC | PROT_READ | PROT_WRITE);
    if (returnval != 0) {
      perror("mprotect");
      cerr << hex << "VDSO page is at " << vdso_begin << endl;
      abort();
    }

    // offset of __kernel_vsyscall() is slightly later under RHEL6 than under RHEL5
    unsigned vsyscall_offset = rhel6 ? 0x420 : 0x400;

    // write int80 at the begining of __kernel_vsyscall()
    *(char *)(vdso_begin + vsyscall_offset + 0) = 0xcd;
    *(char *)(vdso_begin + vsyscall_offset + 1) = 0x80;

    // ... and follow it by a ret
    *(char *)(vdso_begin + vsyscall_offset + 2) = 0xc3;
}

VOID doLateILDJITInstrumentation()
{
  static bool calledAlready = false;

  ASSERTX(!calledAlready);

  PIN_LockClient();
  INS_AddInstrumentFunction(Instrument, 0);
  CODECACHE_FlushCache();
  PIN_UnlockClient();

  calledAlready = true;
}

VOID disable_consumers()
{
  host_cpus = 16;
  if(host_cpus < num_threads * 2) {
    consumers_sleep = true;
  }
}

VOID disable_producers()
{
  host_cpus = 16;
  if(host_cpus < num_threads * 2) {
    producers_sleep = true;
  }
}

VOID enable_consumers()
{
  consumers_sleep = false;
}

VOID enable_producers()
{
  producers_sleep = false;
}
 
VOID flushLookahead(THREADID tid, int numToIgnore) {
  int remainingToIgnore = numToIgnore;
  if(remainingToIgnore > lookahead_buffer[tid].size()) {
    remainingToIgnore = lookahead_buffer[tid].size();
  }

  set<ADDRINT> ignore_pcs;
  if(remainingToIgnore > 0) {   
    // If there's anything at all to ignore, there's at least a call instruction
    ADDRINT last_pc = lookahead_buffer[tid].back()->handshake.pc;
    assert(pc_diss[last_pc].find("call") != string::npos);
    ignore_pcs.insert(last_pc);
    remainingToIgnore--;

    // Now check backwards in time, looking for stack accesses
    // Ignore the most recent stack accesses, assume they are the parameters
    int back_index = lookahead_buffer[tid].size() - 1;
    for(int i = back_index - 1; i >= 0; i--) {
      ADDRINT pc = lookahead_buffer[tid][i]->handshake.pc;
      string diss = pc_diss[pc];
      bool hasMov = diss.find("mov") != string::npos;
      bool hasEsp = diss.find("esp") != string::npos;
      if(hasMov && hasEsp) {
	ignore_pcs.insert(pc);
	remainingToIgnore--;
	if(remainingToIgnore == 0) {
	  break;
	}
      }
    }
    if(remainingToIgnore != 0) {
      for(int i = 0; i < lookahead_buffer[tid].size(); i++) {
	ADDRINT pc = lookahead_buffer[tid][i]->handshake.pc;
	string diss = pc_diss[pc];
	std::cerr << thread_cores[tid] << " "  << hex << pc << dec << " " << diss << endl;
      }
    }
    assert(remainingToIgnore == 0);
  }

  // Perform the flush from lookahead buffer to handshake buffer
  while(!(lookahead_buffer[tid].empty())) {
    handshake_container_t *handshake = lookahead_buffer[tid].front();
    if(ignore_pcs.count(handshake->handshake.pc) == 0) {
      flushOneToHandshakeBuffer(tid);
    }
    else {
#ifdef PRINT_DYN_TRACE 
      printTrace("jit", handshake->handshake.pc, tid); 
#endif
      lookahead_buffer[tid].pop();
    }
  }
}

void flushOneToHandshakeBuffer(THREADID tid)
{
  handshake_container_t *handshake = lookahead_buffer[tid].front();
#ifdef PRINT_DYN_TRACE 
  if(pc_diss[handshake->handshake.pc].find("ret") != string::npos) {
    if (handshake->handshake.npc == handshake->handshake.pc + 5) {
      pc_diss[handshake->handshake.pc] = "Wait";
    }
    else if (handshake->handshake.npc == handshake->handshake.pc + 10) {
      pc_diss[handshake->handshake.pc] = "Signal";
    }
  }
  printTrace("sim", handshake->handshake.pc, tid); 
#endif
  handshake_container_t* newhandshake = handshake_buffer.get_buffer(tid);
  bool isFirstInsn = newhandshake->flags.isFirstInsn;
  handshake->CopyTo(newhandshake);
  newhandshake->flags.isFirstInsn = isFirstInsn;
  handshake_buffer.producer_done(tid);
  lookahead_buffer[tid].pop();
}

VOID printTrace(string stype, ADDRINT pc, THREADID tid)
{
  if(ExecMode != EXECUTION_MODE_SIMULATE) {
    return;
  }

  lk_lock(&instrument_tid_lock, 1);
  pc_file << thread_cores[tid] << " " << stype << " " << pc << " " << pc_diss[pc] << endl;
  lk_unlock(&instrument_tid_lock);
}

