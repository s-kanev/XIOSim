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
#include <syscall.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <elf.h>

#include <unistd.h>

#ifdef TIME_TRANSPARENCY
#include "rdtsc.h"
#endif

#include "feeder.h"
#include "fpstate.h"
#include "ildjit.h"


using namespace std;

/* ========================================================================== */
/* ========================================================================== */
/*                           ZESTO and PIN INTERFACE                          */
/* ========================================================================== */
/* ========================================================================== */

CHAR sim_name[] = "Zesto";

KNOB<UINT64> KnobMaxSimIns(KNOB_MODE_WRITEONCE,    "pintool",
        "maxins", "0", "Max. # of instructions to simulate (0 == till end of program");
KNOB<string> KnobInsTraceFile(KNOB_MODE_WRITEONCE,   "pintool",
        "trace", "", "File where instruction trace is written");
KNOB<string> KnobSanityInsTraceFile(KNOB_MODE_WRITEONCE,   "pintool",
        "sanity_trace", "", "Instruction trace file to use for sanity checking of codepaths");
KNOB<BOOL> KnobSanity(KNOB_MODE_WRITEONCE,     "pintool",
        "sanity", "false", "Sanity-check if simulator corrupted memory (expensive)");
KNOB<BOOL> KnobILDJIT(KNOB_MODE_WRITEONCE,      "pintool",
        "ildjit", "false", "Application run is ildjit");
KNOB<string> KnobFluffy(KNOB_MODE_WRITEONCE,      "pintool",
        "fluffy_annotations", "", "Annotation file that specifies fluffy ROI");

map<ADDRINT, UINT8> sanity_writes;

#ifdef TIME_TRANSPARENCY
// Tracks the time we spend in simulation and tries to subtract it from timing calls
UINT64 sim_time = 0;
#endif

ofstream trace_file;
ifstream sanity_trace;

BOOL isFirstInsn = true;
BOOL isLastInsn = false;

BOOL sim_running = false;

// Used to access thread-local storage
static TLS_KEY tls_key;
static PIN_LOCK test;

// Used to access Zesto instruction buffer
static PIN_LOCK simbuffer_lock;
static map<THREADID, handshake_container_t*> handshake_buffer;

// Used to manage internal pin simulator threads
static queue<THREADID> instrument_tid_queue;
static PIN_LOCK instrument_tid_lock;

// The core where a new thread will be started
UINT32 nextCoreID = 0;

/* ========================================================================== */
/* Pinpoint related */
// Track the number of instructions executed
ICOUNT icount;

// Contains knobs and instrumentation to recognize start/stop points
CONTROL control;

EXECUTION_MODE ExecMode = EXECUTION_MODE_INVALID;

typedef pair <UINT32, CHAR **> SSARGS;

/* ========================================================================== */
UINT64 SimOrgInsCount;                   // # of simulated instructions

/* ========================================================================== */
VOID MakeInsCopy(P2Z_HANDSHAKE* handshake, ADDRINT pc)
{
    memset(handshake->ins, 0, sizeof(handshake->ins));
    memcpy(handshake->ins, reinterpret_cast <VOID *> (pc), sizeof(handshake->ins));
}

// function to access thread-specific data
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

    switch(ev)
    {
      case CONTROL_START:
        cerr << "Start" << endl;
        ExecMode = EXECUTION_MODE_SIMULATE;
        CODECACHE_FlushCache();
//        PIN_RemoveInstrumentation();
        isFirstInsn = true;

        if(control.PinPointsActive())
        {
            cerr << "PinPoint: " << control.CurrentPp(tid) << " PhaseNo: " << control.CurrentPhase(tid) << endl;
        }
//        if (ctxt) PIN_ExecuteAt(ctxt);
        break;

      case CONTROL_STOP:
        cerr << "Stop" << endl;
        ExecMode = EXECUTION_MODE_FASTFORWARD;
        CODECACHE_FlushCache();
//        PIN_RemoveInstrumentation();
        isLastInsn = true;

        if(control.PinPointsActive())
        {
            cerr << "PinPoint: " << control.CurrentPp(tid) << endl;
            tstate->slice_num = control.CurrentPp(tid);
            tstate->slice_length = control.CurrentPpLength(tid);
            tstate->slice_weight_times_1000 = control.CurrentPpWeightTimesThousand(tid);
        }
        else
        {
            /* There will be no further instructions instrumented, make sure we invoke Zesto_Resume with slice_end */
            tstate->handshake.slice_num = 0;
            tstate->handshake.feeder_slice_length = SimOrgInsCount;
            tstate->handshake.slice_weight_times_1000 = 100000;
            Zesto_Resume(&tstate->handshake, false, true);
        }
//        if (ctxt) PIN_ExecuteAt(ctxt);
        break;

      default:
        ASSERTX(false);
        break;
    }
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
VOID MakeSSContext(const CONTEXT *ictxt, const FPSTATE* fpstate, ADDRINT pc, ADDRINT npc, regs_t *ssregs)
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
    //XXX: fpstate is passed from earlier routines, so we don't need to
    // (potentially) call fxsave multiple times

    //Copy the floating point control word
    memcpy(&ssregs->regs_C.cwd, &fpstate->fxsave_legacy._fcw, 2);

    // Copy the floating point status word
    memcpy(&ssregs->regs_C.fsw, &fpstate->fxsave_legacy._fsw, 2);

    //Copy floating point tag word specifying which regsiters hold valid values
    memcpy(&ssregs->regs_C.ftw, &fpstate->fxsave_legacy._ftw, 1);

    #define FXSAVE_STx_OFFSET(arr, st) ((arr) + ((st) * 16))

    //For Zesto, regs_F is indexed by physical register, not stack-based
    #define ST2P(num) ((FSW_TOP(ssregs->regs_C.fsw) + (num)) & 0x7)

    // Copy actual extended fp registers
    memcpy(&ssregs->regs_F.e[ST2P(MD_REG_ST0)], FXSAVE_STx_OFFSET(fpstate->fxsave_legacy._st, MD_REG_ST0), MD_FPR_SIZE);
    memcpy(&ssregs->regs_F.e[ST2P(MD_REG_ST1)], FXSAVE_STx_OFFSET(fpstate->fxsave_legacy._st, MD_REG_ST1), MD_FPR_SIZE);
    memcpy(&ssregs->regs_F.e[ST2P(MD_REG_ST2)], FXSAVE_STx_OFFSET(fpstate->fxsave_legacy._st, MD_REG_ST2), MD_FPR_SIZE);
    memcpy(&ssregs->regs_F.e[ST2P(MD_REG_ST3)], FXSAVE_STx_OFFSET(fpstate->fxsave_legacy._st, MD_REG_ST3), MD_FPR_SIZE);
    memcpy(&ssregs->regs_F.e[ST2P(MD_REG_ST4)], FXSAVE_STx_OFFSET(fpstate->fxsave_legacy._st, MD_REG_ST4), MD_FPR_SIZE);
    memcpy(&ssregs->regs_F.e[ST2P(MD_REG_ST5)], FXSAVE_STx_OFFSET(fpstate->fxsave_legacy._st, MD_REG_ST5), MD_FPR_SIZE);
    memcpy(&ssregs->regs_F.e[ST2P(MD_REG_ST6)], FXSAVE_STx_OFFSET(fpstate->fxsave_legacy._st, MD_REG_ST6), MD_FPR_SIZE);
    memcpy(&ssregs->regs_F.e[ST2P(MD_REG_ST7)], FXSAVE_STx_OFFSET(fpstate->fxsave_legacy._st, MD_REG_ST7), MD_FPR_SIZE);
}

/* ========================================================================== */
VOID Fini(INT32 exitCode, VOID *v)
{
    // Stops simulator threads (good idea to lock around this?)
    sim_running = false;

    Zesto_Destroy();

    cerr << "Total simulated ins = " << dec << SimOrgInsCount << endl;

    if (exitCode != EXIT_SUCCESS)
        cerr << "ERROR! Exit code = " << dec << exitCode << endl;
    cerr << "Total ins: " << icount.Count(0) << endl;
}

/* ========================================================================== */
VOID ExitOnMaxIns()
{
    if(KnobMaxSimIns.Value() == 0)
        return;

    if (KnobMaxSimIns.Value() && (SimOrgInsCount < KnobMaxSimIns.Value()))
        return;

#ifdef ZESTO_PIN_DBG
    cerr << "TotalIns = " << dec << SimOrgInsCount << endl;
#endif

    Fini(EXIT_SUCCESS, 0);

    exit(EXIT_SUCCESS);
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
/* ========================================================================== */
VOID SimulatorLoop(VOID* arg)
{
    THREADID instrument_tid = reinterpret_cast<THREADID>(arg);
    THREADID tid = PIN_ThreadId();

    while (true)
    {
        GetLock(&simbuffer_lock, tid+1);
        if (!sim_running)
        {
            ReleaseLock(&simbuffer_lock);
            break;
        }

        handshake_container_t* handshake = handshake_buffer[instrument_tid];

        while (!handshake->valid)
        {
            ReleaseLock(&simbuffer_lock);
            PIN_Yield();
            GetLock(&simbuffer_lock, tid+1);
        }

        ADDRINT pc = handshake->handshake.pc;

        MakeInsCopy(&handshake->handshake, pc);
        ASSERT(handshake->handshake.orig, "Must execute real instruction in this function");

#ifdef TIME_TRANSPARENCY
        // Capture time spent in simulation to ensure time syscall transparency
        UINT64 ins_delta_time = rdtsc();
#endif
        // Actual simulation happens here
        Zesto_Resume(&handshake->handshake, isFirstInsn, isLastInsn);

        if (isFirstInsn)
            isFirstInsn = false;

        if (isLastInsn)
            isLastInsn = false;

        SimOrgInsCount++;

#ifdef TIME_TRANSPARENCY
        ins_delta_time = rdtsc() - ins_delta_time;
        sim_time += ins_delta_time;
#endif
        // Perform memory sanity checks for values touched by simulator
        if (KnobSanity.Value())
            SanityMemCheck();

        handshake->valid = false;   // Let pin instrument instruction

        ReleaseLock(&simbuffer_lock);
    }
}

/* ========================================================================== */
VOID MakeSSRequest(THREADID tid, ADDRINT pc, ADDRINT npc, ADDRINT tpc, BOOL brtaken, const CONTEXT *ictxt)
{
    handshake_container_t* hshake = handshake_buffer[tid];

    // Must invalidate prior to use because previous invocation data still
    // resides in this statically allocated buffer
    memset(&hshake->handshake, 0, sizeof(P2Z_HANDSHAKE));

    thread_state_t* tstate = get_tls(tid);
    MakeSSContext(ictxt, &tstate->fpstate_buf, pc, npc, &hshake->regstate);

    hshake->handshake.coreID = tstate->coreID;
    hshake->handshake.pc = pc;
    hshake->handshake.npc = npc;
    hshake->handshake.tpc = tpc;
    hshake->handshake.brtaken = brtaken;
    hshake->handshake.ctxt = &hshake->regstate;
    hshake->handshake.orig = TRUE;
    hshake->handshake.icount = 0;

    hshake->handshake.slice_num = tstate->slice_num;
    hshake->handshake.feeder_slice_length = tstate->slice_length;
    hshake->handshake.slice_weight_times_1000 = tstate->slice_weight_times_1000; 
}

/* ========================================================================== */
VOID SimulateInstruction(THREADID tid, ADDRINT pc, BOOL taken, ADDRINT npc, ADDRINT tpc, const CONTEXT *ictxt)
{
    GetLock(&simbuffer_lock, tid+1);
    handshake_container_t* handshake  = handshake_buffer[tid];

    while (handshake->valid)
    {
        ReleaseLock(&simbuffer_lock);
        PIN_Yield();
        GetLock(&simbuffer_lock, tid+1);
    }

    // Tracing
//    if (!KnobInsTraceFile.Value().empty())
//         trace_file << pc << endl;

    // Sanity trace
    if (!KnobSanityInsTraceFile.Value().empty())
    {
        ADDRINT sanity_pc;
        sanity_trace >> sanity_pc;
#ifdef ZESTO_PIN_DBG
        if (sanity_pc != pc)
        {
            cerr << "Sanity_pc != pc. Bad things will happen!" << endl << "sanity_pc: " <<
            hex << sanity_pc << " pc: " << pc << " sim_icount: " << dec << SimOrgInsCount << endl;
        }
#endif
        ASSERTX(sanity_pc == pc);
    }

    if (isFirstInsn)
    {
        thread_state_t* tstate = get_tls(tid);
        Zesto_SetBOS(0/*coreID*/, tstate->bos);
    }

    // Populate handshake buffer
    MakeSSRequest(tid, pc, npc, tpc, taken, ictxt);

    // Clear memory sanity check buffer - callbacks should fill it in SimulatorLoop
    if (KnobSanity.Value())
        sanity_writes.clear();

    // Let simulator consume instruction from SimulatorLoop
    handshake->valid = true;
    ReleaseLock(&simbuffer_lock);
    ExitOnMaxIns();
}
/*======================================================== */
//Check if instruction requires FPState
BOOL TouchesFPState(INS ins)
{
    xed_extension_enum_t ext = static_cast<xed_extension_enum_t>(INS_Extension(ins));
    switch (ext) {
      case XED_EXTENSION_3DNOW:
      case XED_EXTENSION_AES:
      case XED_EXTENSION_AVX:
      case XED_EXTENSION_MMX:
      case XED_EXTENSION_PCLMULQDQ:
      case XED_EXTENSION_SSE:
      case XED_EXTENSION_SSE2:
      case XED_EXTENSION_SSE3:
      case XED_EXTENSION_SSE4:
      case XED_EXTENSION_SSE4A:
      case XED_EXTENSION_SSSE3:
      case XED_EXTENSION_X87:
      case XED_EXTENSION_XSAVE:
      case XED_EXTENSION_XSAVEOPT:
        return true;
      default:
        return false;
    }
}

/*======================================================== */
//Save FP state on the actual hardware
VOID SaveFPState(THREADID tid)
{
    thread_state_t *tstate = get_tls(tid);
    fxsave(reinterpret_cast<char*>(&tstate->fpstate_buf.fxsave_legacy));
}

/*======================================================== */
//Restore FP state on the actual hardware
VOID RestoreFPState(THREADID tid)
{
    thread_state_t *tstate = get_tls(tid);
    fxrstor(reinterpret_cast<char*>(&tstate->fpstate_buf.fxsave_legacy));
}

/* ========================================================================== */
//Trivial call to let us do conditional instrumentation based on an argument
ADDRINT returnArg(BOOL arg)
{
   return arg;
}

/* ========================================================================== */
VOID Instrument(INS ins, VOID *v)
{
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;

    // ILDJIT is doing its initialization/compilation/...
    if (KnobILDJIT.Value() && !ILDJIT_IsExecuting())
        return;

    // Tracing
    if (!KnobInsTraceFile.Value().empty()) {
             ADDRINT pc = INS_Address(ins);
             USIZE size = INS_Size(ins);

             trace_file << pc << " " << INS_Disassemble(ins);
             for (INT32 curr = size-1; curr >= 0; curr--)
                trace_file << " " << int(*(UINT8*)(curr + pc));
             trace_file << endl;
    }

    // Save FP state since SimulateInstruction may corrupt it
    // Note: PIN ensures proper order of instrument functions
    // Note: MakeSSContext relies this was called, so don't make
    // it conditional
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) SaveFPState,
                    IARG_THREAD_ID,
                    IARG_END);

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
                       IARG_CONTEXT,
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
                       IARG_CONTEXT,
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
                   IARG_CONTEXT,
                   IARG_END);
    }

    
    // Now restore FP state, so we don't corrupt user application state
    // TODO: this is costly, so only do if ins will need correct fpstate
//    if (TouchesFPState(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) RestoreFPState,
                       IARG_THREAD_ID,
                       IARG_END); 
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
VOID ThreadStart(THREADID threadIndex, CONTEXT * ictxt, INT32 flags, VOID *v)
{
    // ILDJIT is forking a compiler thread, ignore
//    if (KnobILDJIT.Value() && !ILDJIT_IsCreatingExecutor())
//        return;

    GetLock(&test, 1);

    cerr << "Thread start. ID: " << dec << threadIndex << endl;

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

    // Application threads only -- assign a core for them
    // and make sure a respective simulator thread gets created
    if (!KnobILDJIT.Value() ||
        (KnobILDJIT.Value() && ILDJIT_IsCreatingExecutor()))
    {
        tstate->coreID = nextCoreID++;

        // Create new buffer to store thread context
        GetLock(&simbuffer_lock, threadIndex+1);
        handshake_container_t* new_handshake = new handshake_container_t();
        handshake_buffer[threadIndex] = new_handshake;
        ReleaseLock(&simbuffer_lock);

        // Mark simulation as running (only matters for first thread)
        sim_running = true;

        // This will trigger spawning a sim thread
        GetLock(&instrument_tid_lock, threadIndex+1);
        instrument_tid_queue.push(threadIndex); 
        ReleaseLock(&instrument_tid_lock);
    }

    ReleaseLock(&test);
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
    GetLock(&test, threadIndex+1);

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

      default:
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall " << dec << syscall_num << endl;
#endif
        break;
    }
    ReleaseLock(&test);
}

/* ========================================================================== */
VOID SyscallExit(THREADID threadIndex, CONTEXT * ictxt, SYSCALL_STANDARD std, VOID *v)
{
    // ILDJIT is minding its own bussiness
//    if (KnobILDJIT.Value() && !ILDJIT_IsExecuting())
//        return;

    GetLock(&test, threadIndex+1);
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
            if ((tstate->last_syscall_arg3 & PROT_READ) == 0)
                ASSERTX( Zesto_Notify_Munmap(0/*coreID*/, tstate->last_syscall_arg1, tstate->last_syscall_arg2, false) );
            else
                ASSERTX( Zesto_Notify_Mmap(0/*coreID*/, tstate->last_syscall_arg1, tstate->last_syscall_arg2, false) );
        break;

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
    ReleaseLock(&test);
}

/* ========================================================================== */
VOID SimulatorThreadSpawner(VOID* arg)
{
    (void) arg;

    while (true)
    {
        // Is it time to die?
        if (PIN_IsProcessExiting())
            break;

        // Check if there's an insturment thread without the appropriate simulator thread
        // and spawn one, if necessary
        GetLock(&instrument_tid_lock, 1);
        if (!instrument_tid_queue.empty())
        {
            THREADID instrument_tid = instrument_tid_queue.front();
            instrument_tid_queue.pop();
            PIN_SpawnInternalThread(SimulatorLoop, reinterpret_cast<VOID*>(instrument_tid),
                                    0, NULL);
        }
        ReleaseLock(&instrument_tid_lock);

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

    InitLock(&test);
    InitLock(&simbuffer_lock);
    InitLock(&instrument_tid_lock);

    // Obtain  a key for TLS storage.
    tls_key = PIN_CreateThreadDataKey(0);

    PIN_Init(argc, argv);
    PIN_InitSymbols();

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

    PIN_AddThreadStartFunction(ThreadStart, NULL);  
    IMG_AddUnloadFunction(ImageUnload, 0);
    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_AddSyscallEntryFunction(SyscallEntry, 0);
    PIN_AddSyscallExitFunction(SyscallExit, 0);
    INS_AddInstrumentFunction(Instrument, 0);
    PIN_AddFiniFunction(Fini, 0);

    if(KnobSanity.Value())
        Zesto_Add_WriteByteCallback(Zesto_WriteByteCallback);

    Zesto_SlaveInit(ssargs.first, ssargs.second);

    // The only safe way to spawn internal pin threads is from main() 
    // or other internal threads, so we create a thread spawner here
    PIN_SpawnInternalThread(SimulatorThreadSpawner, NULL, 0, NULL);

    PIN_StartProgram();

    return 0;
}

/* ========================================================================== */
/* Wrappers for PIN-locking, so we can build Zesto as a standalone lib */

void lk_lock(int32_t* lk)
{
    GetLock(lk, 1);
}

int32_t lk_unlock(int32_t* lk)
{
    return ReleaseLock(lk);
}

void lk_init(int32_t* lk)
{
    InitLock(lk);
}

void spawn_new_thread(void entry_point(void*), void* arg)
{
    PIN_SpawnInternalThread(entry_point, arg, 0, NULL);
}
