/* ========================================================================== */
/* ========================================================================== */
/*                      
    Feeder to Zesto.
*/
/* ========================================================================== */
/* ========================================================================== */

#include <iostream>
#include <iomanip>
#include <map>
#include <syscall.h>
#include <stdlib.h>
#include <elf.h>

#include <unistd.h>

#include "pin.H"
#include "instlib.H"

#ifdef TIME_TRANSPARENCY
#include "rdtsc.h"
#endif

#include "fpstate.h"

#include "../interface.h" 

using namespace std;
using namespace INSTLIB;

/* ========================================================================== */
/* ========================================================================== */
/*                           ZESTO and PIN INTERFACE                          */
/* ========================================================================== */
/* ========================================================================== */

KNOB<UINT64> KnobFFwd(KNOB_MODE_WRITEONCE,    "pintool",
        "ffwd", "0", "Number of instructions to fast forward");
KNOB<UINT64> KnobMaxSimIns(KNOB_MODE_WRITEONCE,    "pintool",
        "maxins", "0", "Max. # of instructions to simulate (0 == till end of program");
KNOB<string> KnobInsTraceFile(KNOB_MODE_WRITEONCE,   "pintool",
        "trace", "", "File where instruction trace is written");
KNOB<string> KnobSanityInsTraceFile(KNOB_MODE_WRITEONCE,   "pintool",
        "sanity_trace", "", "Instruction trace file to use for sanity checking of codepaths");
KNOB<BOOL> KnobSanity(KNOB_MODE_WRITEONCE,     "pintool",
        "sanity", "false", "Sanity-check if simulator corrupted memory (expensive)");

map<ADDRINT, UINT8> sanity_writes;

#ifdef TIME_TRANSPARENCY
// Tracks the time we spend in simulation and tries to subtract it from timing calls
UINT64 sim_time = 0;
#endif

ofstream trace_file;
ifstream sanity_trace;

BOOL isFirstInsn = true;
BOOL isLastInsn = false;

// Buffer to store FP state
static FPSTATE fpstate_buf;

/* ========================================================================== */
/* Pinpoint related */
// Track the number of instructions executed
ICOUNT icount;

// Contains knobs and instrumentation to recognize start/stop points
CONTROL control;

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

EXECUTION_MODE ExecMode = EXECUTION_MODE_INVALID;

typedef pair <UINT32, CHAR **> SSARGS;

/* ========================================================================== */
INSTLIB::ALARM_ICOUNT FFwdingAlarm; // Fires upon reaching point of interest
UINT64 SimOrgInsCount;                   // # of simulated instructions

/* ========================================================================== */
unsigned char * MakeCopy(ADDRINT pc)
{
    STATIC unsigned char codeBuff[16];

    memset(codeBuff, 0, sizeof(codeBuff));
    memcpy(codeBuff, reinterpret_cast <VOID *> (pc), sizeof(codeBuff));

    return codeBuff;
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

    ASSERTX( Zesto_Notify_Munmap(start, length, true));
}

/* ========================================================================== */
VOID PPointHandler(CONTROL_EVENT ev, VOID * v, CONTEXT * ctxt, VOID * ip, THREADID tid)
{
    cerr << "tid: " << dec << tid << " ip: 0x" << hex << ip; 
    // get line info on current instruction
    cerr <<  dec << " Inst. Count " << icount.Count(tid) << " ";

    switch(ev)
    {
      case CONTROL_START:
        cerr << "Start" << endl;
        ExecMode = EXECUTION_MODE_SIMULATE;

        if(control.PinPointsActive())
        {
            CODECACHE_FlushCache();
            isFirstInsn = true;
            cerr << "PinPoint: " << control.CurrentPp(tid) << " PhaseNo: " << control.CurrentPhase(tid) << endl;
        }
        break;

      case CONTROL_STOP:
        cerr << "Stop" << endl;
        ExecMode = EXECUTION_MODE_FASTFORWARD;

        if(control.PinPointsActive())
        {
            CODECACHE_FlushCache();
            isLastInsn = true;
            cerr << "PinPoint: " << control.CurrentPp(tid) << endl;
        }
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

    ASSERTX( Zesto_Notify_Mmap(start, length, true));
}

/* ========================================================================== */
/* The returned pointer is to static data that is overwritten with each call */
struct regs_t *MakeSSContext(const CONTEXT *ictxt, const FPSTATE* fpstate, ADDRINT pc, ADDRINT npc)
{
    CONTEXT ssctxt;
    memset(&ssctxt, 0x0, sizeof(ssctxt));
    PIN_SaveContext(ictxt, &ssctxt);

    // Must invalidate prior to use because previous invocation data still
    // resides in this statically allocated buffer
    static struct regs_t SSRegs;
    memset(&SSRegs, 0x0, sizeof(SSRegs));
    
    SSRegs.regs_PC = pc;
    SSRegs.regs_NPC = npc;

    // Copy general purpose registers, which Pin provides individual access to
    SSRegs.regs_C.aflags = PIN_GetContextReg(&ssctxt, REG_EFLAGS);
    SSRegs.regs_R.dw[MD_REG_EAX] = PIN_GetContextReg(&ssctxt, REG_EAX);
    SSRegs.regs_R.dw[MD_REG_ECX] = PIN_GetContextReg(&ssctxt, REG_ECX);
    SSRegs.regs_R.dw[MD_REG_EDX] = PIN_GetContextReg(&ssctxt, REG_EDX);
    SSRegs.regs_R.dw[MD_REG_EBX] = PIN_GetContextReg(&ssctxt, REG_EBX);
    SSRegs.regs_R.dw[MD_REG_ESP] = PIN_GetContextReg(&ssctxt, REG_ESP);
    SSRegs.regs_R.dw[MD_REG_EBP] = PIN_GetContextReg(&ssctxt, REG_EBP);
    SSRegs.regs_R.dw[MD_REG_EDI] = PIN_GetContextReg(&ssctxt, REG_EDI);
    SSRegs.regs_R.dw[MD_REG_ESI] = PIN_GetContextReg(&ssctxt, REG_ESI);


    // Copy segment registers (IA32-specific)
     SSRegs.regs_S.w[MD_REG_CS] = PIN_GetContextReg(&ssctxt, REG_SEG_CS);
     SSRegs.regs_S.w[MD_REG_SS] = PIN_GetContextReg(&ssctxt, REG_SEG_SS);
     SSRegs.regs_S.w[MD_REG_DS] = PIN_GetContextReg(&ssctxt, REG_SEG_DS);
     SSRegs.regs_S.w[MD_REG_ES] = PIN_GetContextReg(&ssctxt, REG_SEG_ES);
     SSRegs.regs_S.w[MD_REG_FS] = PIN_GetContextReg(&ssctxt, REG_SEG_FS);
     SSRegs.regs_S.w[MD_REG_GS] = PIN_GetContextReg(&ssctxt, REG_SEG_GS);


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
    memcpy(&SSRegs.regs_C.cwd, &fpstate->fxsave_legacy._fcw, 2);

    // Copy the floating point status word
    memcpy(&SSRegs.regs_C.fsw, &fpstate->fxsave_legacy._fsw, 2);

    //Copy floating point tag word specifying which regsiters hold valid values
    memcpy(&SSRegs.regs_C.ftw, &fpstate->fxsave_legacy._ftw, 1);

    #define FXSAVE_STx_OFFSET(arr, st) ((arr) + ((st) * 16))

    //For Zesto, regs_F is indexed by physical register, not stack-based
    #define ST2P(num) ((FSW_TOP(SSRegs.regs_C.fsw) + (num)) & 0x7)

    // Copy actual extended fp registers
    memcpy(&SSRegs.regs_F.e[ST2P(MD_REG_ST0)], FXSAVE_STx_OFFSET(fpstate->fxsave_legacy._st, MD_REG_ST0), MD_FPR_SIZE);
    memcpy(&SSRegs.regs_F.e[ST2P(MD_REG_ST1)], FXSAVE_STx_OFFSET(fpstate->fxsave_legacy._st, MD_REG_ST1), MD_FPR_SIZE);
    memcpy(&SSRegs.regs_F.e[ST2P(MD_REG_ST2)], FXSAVE_STx_OFFSET(fpstate->fxsave_legacy._st, MD_REG_ST2), MD_FPR_SIZE);
    memcpy(&SSRegs.regs_F.e[ST2P(MD_REG_ST3)], FXSAVE_STx_OFFSET(fpstate->fxsave_legacy._st, MD_REG_ST3), MD_FPR_SIZE);
    memcpy(&SSRegs.regs_F.e[ST2P(MD_REG_ST4)], FXSAVE_STx_OFFSET(fpstate->fxsave_legacy._st, MD_REG_ST4), MD_FPR_SIZE);
    memcpy(&SSRegs.regs_F.e[ST2P(MD_REG_ST5)], FXSAVE_STx_OFFSET(fpstate->fxsave_legacy._st, MD_REG_ST5), MD_FPR_SIZE);
    memcpy(&SSRegs.regs_F.e[ST2P(MD_REG_ST6)], FXSAVE_STx_OFFSET(fpstate->fxsave_legacy._st, MD_REG_ST6), MD_FPR_SIZE);
    memcpy(&SSRegs.regs_F.e[ST2P(MD_REG_ST7)], FXSAVE_STx_OFFSET(fpstate->fxsave_legacy._st, MD_REG_ST7), MD_FPR_SIZE);


    return &SSRegs;
}

/* ========================================================================== */
VOID Fini(INT32 exitCode, VOID *v)
{
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
VOID FeedOriginalInstruction(struct P2Z_HANDSHAKE *handshake)
{
    ADDRINT pc = handshake->pc;

    handshake->ins = MakeCopy(pc);
    ASSERT(handshake->orig, "Must execute real instruction in this function");

#ifdef TIME_TRANSPARENCY
    // Capture time spent in simulation to ensure time syscall transparency
    UINT64 ins_delta_time = rdtsc();
#endif

    Zesto_Resume(handshake, isFirstInsn, isLastInsn);

#ifdef TIME_TRANSPARENCY
    ins_delta_time = rdtsc() - ins_delta_time;
    sim_time += ins_delta_time;
#endif

    if(isFirstInsn)
        isFirstInsn = false;

    if(isLastInsn)
        isLastInsn = false;

    SimOrgInsCount++;
}

/* ========================================================================== */
struct P2Z_HANDSHAKE *MakeSSRequest(ADDRINT pc, ADDRINT npc, ADDRINT tpc, BOOL brtaken, const CONTEXT *ictxt, const FPSTATE* fpstate)
{
    // Must invalidate prior to use because previous invocation data still
    // resides in this statically allocated buffer
    static struct P2Z_HANDSHAKE handshake;
    memset(&handshake, 0, sizeof(P2Z_HANDSHAKE));

    handshake.pc = pc;
    handshake.npc = npc;
    handshake.tpc = tpc;
    handshake.brtaken = brtaken;
    handshake.ctxt = MakeSSContext(ictxt, fpstate, pc, npc);
    handshake.orig = TRUE;
    handshake.ins = NULL;
    handshake.icount = 0;
    return &handshake;
}

/* ========================================================================== */
VOID SimulateInstruction(ADDRINT pc, BOOL taken, ADDRINT npc, ADDRINT tpc, const CONTEXT *ictxt)
{
    // Tracing
    if (!KnobInsTraceFile.Value().empty())
         trace_file << pc << endl;

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

    struct P2Z_HANDSHAKE *handshake  = MakeSSRequest(pc, npc, tpc, taken, ictxt, &fpstate_buf);

    // Clear memory sanity check buffer - callbacks should fill it in FeedOriginalInstruction
    if (KnobSanity.Value())
        sanity_writes.clear();

    // Pass instruction to simulator
    FeedOriginalInstruction(handshake);

    // Perform memory sanity checks for values touched by simulator
    if (KnobSanity.Value())
        SanityMemCheck();

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
VOID SaveFPState()
{
   fxsave(reinterpret_cast<char*>(&fpstate_buf.fxsave_legacy));
}

/*======================================================== */
//Restore FP state on the actual hardware
VOID RestoreFPState()
{
   fxrstor(reinterpret_cast<char*>(&fpstate_buf.fxsave_legacy));
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

    // Save FP state since SimulateInstruction may corrupt it
    // Note: PIN ensures proper order of instrument functions
    // Note: MakeSSContext relies this was called, so don't make
    // it conditional
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) SaveFPState,
                    IARG_END);

    if (! INS_IsBranchOrCall(ins))
    {
        // REP-ed instruction: only instrument first iteration
        // (simulator will return once all iterations are done)
        if(INS_HasRealRep(ins))
        {
           INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
           INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR) SimulateInstruction, 
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
                        IARG_END); 
}

/* ========================================================================== */
VOID FFwdHandler(VOID * val, CONTEXT * ctxt, VOID * ip, THREADID tid)
{
    INSTLIB::ALARM_ICOUNT * al = static_cast<INSTLIB::ALARM_ICOUNT *> (val);

    al->DeActivate();

    CODECACHE_FlushCache();

    ExecMode = EXECUTION_MODE_SIMULATE;
    isFirstInsn = true;
}
/* ========================================================================== */
VOID InstallFastForwarding()
{
    FFwdingAlarm.Activate();

    FFwdingAlarm.SetAlarm(KnobFFwd.Value(), FFwdHandler, &FFwdingAlarm);

    ExecMode = EXECUTION_MODE_FASTFORWARD;
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
    ssArgv[ssArgIndex++] = "SimpleScalar";  // Does not matter; just for sanity
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
VOID onMainThreadStart(THREADID threadIndex, CONTEXT * ictxt, INT32 flags, VOID *v)
{
//    cout << "Thread start. ID: " << threadIndex << endl;
    if(threadIndex != 0)
      return;


    CHAR* sp = (CHAR*)PIN_GetContextReg(ictxt, REG_ESP);
//    cout << hex << "SP: " << (VOID*) sp << dec << endl;
    
    while(*sp++); //go to end of argv
//    cout << hex << (VOID*)sp << dec << endl;
    while(*sp++); //go to end of envp

    Elf32_auxv_t *auxv;
    for(auxv = (Elf32_auxv_t*)sp; auxv->a_type != AT_NULL; auxv++); //go to end of aux_vector

    INT32* bos = (INT32*)((CHAR*)auxv+1);

    while(*bos++); //this should reach bottom of stack

    Zesto_SetBOS((ADDRINT) bos);
//    cout << (int)*sp << endl;
//    cout << hex << (VOID*)sp << dec << endl;
//    cout << *(char*)sp << endl;

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

ADDRINT last_syscall_number;
ADDRINT last_syscall_arg1;
ADDRINT last_syscall_arg2;
ADDRINT last_syscall_arg3;

/* ========================================================================== */
VOID SyscallEntry(THREADID threadIndex, CONTEXT * ictxt, SYSCALL_STANDARD std, VOID *v)
{
    //Single-threaded for now
    ASSERTX(threadIndex == 0);

    ADDRINT syscall_num = PIN_GetSyscallNumber(ictxt, std);
    ADDRINT arg1 = PIN_GetSyscallArgument(ictxt, std, 0);
    ADDRINT arg2;
    ADDRINT arg3;
    mmap_arg_struct mmap_arg;

    last_syscall_number = syscall_num;

    switch(syscall_num)
    {
      case __NR_brk:
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall brk(" << dec << syscall_num << ") addr: 0x" << hex << arg1 << dec << endl;
#endif
        last_syscall_arg1 = arg1;
        break;

      case __NR_munmap:
        arg2 = PIN_GetSyscallArgument(ictxt, std, 1);
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall munmap(" << dec << syscall_num << ") addr: 0x" << hex << arg1 
             << " length: " << arg2 << dec << endl;
#endif
        last_syscall_arg1 = arg1;
        last_syscall_arg2 = arg2;
        break;

      case __NR_mmap: //oldmmap
        memcpy(&mmap_arg, (void*)arg1, sizeof(mmap_arg_struct));
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall oldmmap(" << dec << syscall_num << ") addr: 0x" << hex << mmap_arg.addr 
             << " length: " << mmap_arg.len << dec << endl;
#endif
        last_syscall_arg1 = mmap_arg.len;
        break;

      case __NR_mmap2:
        arg2 = PIN_GetSyscallArgument(ictxt, std, 1);
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall mmap2(" << dec << syscall_num << ") addr: 0x" << hex << arg1 
             << " length: " << arg2 << dec << endl;
#endif
        last_syscall_arg1 = arg2;
        break;

      case __NR_mremap:
        arg2 = PIN_GetSyscallArgument(ictxt, std, 1);
        arg3 = PIN_GetSyscallArgument(ictxt, std, 2);
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall mremap(" << dec << syscall_num << ") old_addr: 0x" << hex << arg1 
             << " old_length: " << arg2 << " new_length: " << arg3 << dec << endl;
#endif
        last_syscall_arg1 = arg1;
        last_syscall_arg2 = arg2;
        last_syscall_arg3 = arg3;
        break;

#ifdef TIME_TRANSPARENCY
      case __NR_times:
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall times(" << dec << syscall_num << ") num_ins: " << SimOrgInsCount << endl;
#endif
        last_syscall_arg1 = arg1;
        break;
#endif

      default:
#ifdef ZESTO_PIN_DBG
        cerr << "Syscall " << dec << syscall_num << endl;
#endif
        break;
    }
}

/* ========================================================================== */
VOID SyscallExit(THREADID threadIndex, CONTEXT * ictxt, SYSCALL_STANDARD std, VOID *v)
{
    //Single-threaded for now
    ASSERTX(threadIndex == 0);

    ADDRINT retval = PIN_GetSyscallReturn(ictxt, std);

#ifdef TIME_TRANSPARENCY
    //for times()
    tms* buf;
    clock_t adj_time;
#endif

    switch(last_syscall_number)
    {
      case __NR_brk:
#ifdef ZESTO_PIN_DBG
        cerr << "Ret syscall brk(" << dec << last_syscall_number << ") addr: 0x" 
             << hex << retval << dec << endl;
#endif
        if(last_syscall_arg1 != 0)
            Zesto_UpdateBrk(last_syscall_arg1, true);
        /* Seemingly libc code calls sbrk(0) to get the initial value of the sbrk. We intercept that and send result to zesto, so that we can correclty deal with virtual memory. */
        else
            Zesto_UpdateBrk(retval, false);
        break;

      case __NR_munmap:
#ifdef ZESTO_PIN_DBG
        cerr << "Ret syscall munmap(" << dec << last_syscall_number << ") addr: 0x" 
             << hex << last_syscall_arg1 << " length: " << last_syscall_arg2 << dec << endl;
#endif
        Zesto_Notify_Munmap(last_syscall_arg1, last_syscall_arg2, false);
        break;

      case __NR_mmap: //oldmap
#ifdef ZESTO_PIN_DBG
        cerr << "Ret syscall oldmmap(" << dec << last_syscall_number << ") addr: 0x" 
             << hex << retval << " length: " << last_syscall_arg1 << dec << endl;
#endif

        ASSERTX( Zesto_Notify_Mmap(retval, last_syscall_arg1, false) );
        break;

      case __NR_mmap2:
#ifdef ZESTO_PIN_DBG
        cerr << "Ret syscall mmap2(" << dec << last_syscall_number << ") addr: 0x" 
             << hex << retval << " length: " << last_syscall_arg1 << dec << endl;
#endif

        ASSERTX( Zesto_Notify_Mmap(retval, last_syscall_arg1, false) );
        break;

      case __NR_mremap:
#ifdef ZESTO_PIN_DBG
        cerr << "Ret syscall mremap(" << dec << last_syscall_number << ") " << hex 
             << " old_addr: 0x" << last_syscall_arg1
             << " old_length: " << last_syscall_arg2
             << " new address: 0x" << retval
             << " new_length: " << last_syscall_arg3 << dec << endl;
#endif

        ASSERTX( Zesto_Notify_Munmap(last_syscall_arg1, last_syscall_arg2, false) );
        ASSERTX( Zesto_Notify_Mmap(retval, last_syscall_arg3, false) );
        break;

#ifdef TIME_TRANSPARENCY
      case __NR_times:
        buf = (tms*) last_syscall_arg1;
        adj_time = retval - (clock_t) sim_time;
#ifdef ZESTO_PIN_DBG
        cerr << "Ret syscall times(" << dec << last_syscall_number << ") old: "
             << retval  << " adjusted: " << adj_time 
             << " user: " << buf->tms_utime 
             << " user_adj: " << (buf->tms_utime - sim_time)
             << " system: " << buf->tms_stime << endl;
#endif
        /* Compensate for time we spent on simulation
         * Included for full transparency - some apps detect we are taking a long time
         * and do bad things like dropping frames
         * Since we have no decent way of measuring how much time the simulator spends in the OS
         * (other than calling times() for every instruction), we assume the simulator is mainly
         * user code. XXX: how reasonable is this assmuption???
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

    PIN_Init(argc, argv);
    PIN_InitSymbols();


    // Try activate pinpoints alarm, must be done before PIN_StartProgram
    if(control.CheckKnobs(PPointHandler, 0) != 1)
        InstallFastForwarding(); //If not, try activate simple ffwd

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

    PIN_AddThreadStartFunction(onMainThreadStart, NULL);  
    IMG_AddUnloadFunction(ImageUnload, 0);
    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_AddSyscallEntryFunction(SyscallEntry, 0);
    PIN_AddSyscallExitFunction(SyscallExit, 0);
    INS_AddInstrumentFunction(Instrument, 0);
    PIN_AddFiniFunction(Fini, 0);

    if(KnobSanity.Value())
        Zesto_Add_WriteByteCallback(Zesto_WriteByteCallback);

    Zesto_SlaveInit(ssargs.first, ssargs.second);

    PIN_StartProgram();

    return 0;
}
