/* ========================================================================== */
/* ========================================================================== */
/*                      
    Feeder to Zesto.
*/
/* ========================================================================== */
/* ========================================================================== */

#include <iostream>
#include <iomanip>
#include <syscall.h>
#include <stdlib.h>
#include <elf.h>

#include <unistd.h>

#include "pin.H"
#include "instlib.H"

#include "../interface.h" 

using namespace std;

/* ========================================================================== */
/* ========================================================================== */
/*                           ZESTO and PIN INTERFACE                          */
/* ========================================================================== */
/* ========================================================================== */

KNOB<UINT64> KnobFFwd(KNOB_MODE_WRITEONCE,    "pintool",
        "ffwd", "0", "Number of instructions to fast forward");
KNOB<UINT64> KnobMaxSimIns(KNOB_MODE_WRITEONCE,    "pintool",
        "maxins", "100000000", "Max. # of instructions to simulate (0 == till end of program");
KNOB<string> KnobInsTraceFile(KNOB_MODE_WRITEONCE,   "pintool",
        "trace", "", "File where instruction trace is written");

ofstream trace_file;

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

    cout << "Image unload, addr: " << hex << start  
         << " len: " << length << " end_addr: " << start + length << endl;

    ASSERTX( Zesto_Notify_Munmap(start, length, true));
}

/* ========================================================================== */
VOID ImageLoad(IMG img, VOID *v)
{
    ADDRINT start = IMG_LowAddress(img);
    ADDRINT length = IMG_HighAddress(img) - start;

    cout << "Image load, addr: " << hex << start  
         << " len: " << length << " end_addr: " << start + length << endl;

    ASSERTX( Zesto_Notify_Mmap(start, length, true));
}

/* ========================================================================== */
/* The returned pointer is to static data that is overwritten with each call */
struct regs_t *MakeSSContext(const CONTEXT *ictxt, ADDRINT pc, ADDRINT npc)
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
    // header file for the complete layout of the fxsave region. SS only
    // requires the (1) floating point status word, and the (2) eight 10byte
    // floating point registers. Thus, we only copy the required information
    // into the SS-specific data structure
    CHAR fpstate[512];
    memset(&fpstate, 0x0, sizeof(fpstate));

    PIN_GetContextFPState(&ssctxt, &fpstate);

    // Copy the floating point status word, which is @ a 2-byte offset
    memcpy(&SSRegs.regs_C.fsw, fpstate+2, 2);

    #define FXSAVE_STx_OFFSET(arr, st) (arr + 32 + (st * 16))
    #define ST_SIZE (10) // Each x86 floating point register is 80-bits wide

    // Load up the SS-specific data structures
    memcpy(&SSRegs.regs_F.e[MD_REG_ST0], FXSAVE_STx_OFFSET(fpstate, MD_REG_ST0), ST_SIZE);
    memcpy(&SSRegs.regs_F.e[MD_REG_ST1], FXSAVE_STx_OFFSET(fpstate, MD_REG_ST1), ST_SIZE);
    memcpy(&SSRegs.regs_F.e[MD_REG_ST2], FXSAVE_STx_OFFSET(fpstate, MD_REG_ST2), ST_SIZE);
    memcpy(&SSRegs.regs_F.e[MD_REG_ST3], FXSAVE_STx_OFFSET(fpstate, MD_REG_ST3), ST_SIZE);
    memcpy(&SSRegs.regs_F.e[MD_REG_ST4], FXSAVE_STx_OFFSET(fpstate, MD_REG_ST4), ST_SIZE);
    memcpy(&SSRegs.regs_F.e[MD_REG_ST5], FXSAVE_STx_OFFSET(fpstate, MD_REG_ST5), ST_SIZE);
    memcpy(&SSRegs.regs_F.e[MD_REG_ST6], FXSAVE_STx_OFFSET(fpstate, MD_REG_ST6), ST_SIZE);
    memcpy(&SSRegs.regs_F.e[MD_REG_ST7], FXSAVE_STx_OFFSET(fpstate, MD_REG_ST7), ST_SIZE);

    return &SSRegs;
}

/* ========================================================================== */
VOID Fini(INT32 exitCode, VOID *v)
{
    Zesto_Destroy();

    cout << "TotalIns = " << dec << SimOrgInsCount << endl;

    if (exitCode != EXIT_SUCCESS)
        cout << "ERROR! Exit code = " << dec << exitCode << endl;
}

/* ========================================================================== */
VOID ExitOnMaxIns()
{
    if (KnobMaxSimIns.Value() && (SimOrgInsCount < KnobMaxSimIns.Value()))
        return;

    cout << "TotalIns = " << dec << SimOrgInsCount << endl;

    Fini(EXIT_SUCCESS, 0);

    exit(EXIT_SUCCESS);
}

/* ========================================================================== */
VOID FeedOriginalInstruction(struct P2Z_HANDSHAKE *handshake)
{
    ADDRINT pc = handshake->pc;

    if(!KnobInsTraceFile.Value().empty())
         trace_file << pc << endl;

    handshake->ins = MakeCopy(pc);
    ASSERT(handshake->orig, "Must execute real instruction in this function");

    Zesto_Resume(handshake);

    SimOrgInsCount++;
}

/* ========================================================================== */
struct P2Z_HANDSHAKE *MakeSSRequest(ADDRINT pc, ADDRINT npc, ADDRINT tpc, BOOL brtaken, const CONTEXT *ictxt)
{
    // Must invalidate prior to use because previous invocation data still
    // resides in this statically allocated buffer
    static struct P2Z_HANDSHAKE handshake;
    memset(&handshake, 0, sizeof(P2Z_HANDSHAKE));

    handshake.pc = pc;
    handshake.npc = npc;
    handshake.tpc = tpc;
    handshake.brtaken = brtaken;
    handshake.ctxt = MakeSSContext(ictxt, pc, npc);
    handshake.orig = TRUE;
    handshake.ins = NULL;
    handshake.icount = 0;
    return &handshake;
}

/* ========================================================================== */
VOID SimulateInstruction(ADDRINT pc, BOOL taken, ADDRINT npc, ADDRINT tpc, const CONTEXT *ictxt)
{
    struct P2Z_HANDSHAKE *handshake  = MakeSSRequest(pc, npc, tpc, taken, ictxt);

#ifdef ZESTO_PIN_DBG
    cout << SimOrgInsCount << "  PC: " << hex << pc << " Taken br: " << taken << " NPC: " << (taken ? tpc : npc) << dec << endl;
#endif

    FeedOriginalInstruction(handshake);

    ExitOnMaxIns();
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
    if (ExecMode == EXECUTION_MODE_FASTFORWARD)
        return;

#ifdef ZESTO_PIN_DBG
    cout << INS_Mnemonic(ins) << endl;
#endif

    if (! INS_IsBranchOrCall(ins))
    {
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
          INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) SimulateInstruction, 
                       IARG_INST_PTR, 
                       IARG_BRANCH_TAKEN, 
                       IARG_ADDRINT, INS_NextAddress(ins),
                       IARG_BRANCH_TARGET_ADDR, 
                       IARG_CONTEXT,
                       IARG_END);
    } 
}

/* ========================================================================== */
VOID Handler(VOID * val, CONTEXT * ctxt, VOID * ip, THREADID tid)
{
    INSTLIB::ALARM_ICOUNT * al = static_cast<INSTLIB::ALARM_ICOUNT *> (val);

    al->DeActivate();

    CODECACHE_FlushCache();

    ExecMode = EXECUTION_MODE_SIMULATE;
}
 
/* ========================================================================== */
VOID InstallFastForwarding()
{
    FFwdingAlarm.Activate();

    FFwdingAlarm.SetAlarm(KnobFFwd.Value(), Handler, &FFwdingAlarm);

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

ADDRINT last_syscall_number;
ADDRINT last_syscall_arg;

/* ========================================================================== */
VOID SyscallEntry(THREADID threadIndex, CONTEXT * ictxt, SYSCALL_STANDARD std, VOID *v)
{
    //Single-threaded for now
    assert(threadIndex == 0);

    ADDRINT syscall_num = PIN_GetSyscallNumber(ictxt, std);
    ADDRINT addr = PIN_GetSyscallArgument(ictxt, std, 0);

    last_syscall_number = syscall_num;

    if(syscall_num == __NR_brk)
    {
      cout << "Syscall brk(" << syscall_num << ") addr: 0x" << hex << addr << dec << endl;
      Zesto_UpdateBrk(addr);
    } else
    if(syscall_num == __NR_munmap)
    {
      ADDRINT size = PIN_GetSyscallArgument(ictxt, std, 1);
      Zesto_Notify_Munmap(addr, size, false);
      cout << "Syscall munmap(" << syscall_num << ") addr: 0x" << hex << addr 
           << " length: " << size << dec << endl;
    } else
    if(syscall_num == 90) //oldmmap
    {
      mmap_arg_struct arg;
      memcpy(&arg, (void*)addr, sizeof(mmap_arg_struct));
      cout << "Syscall oldmmap(" << syscall_num << ") addr: 0x" << hex << arg.addr 
           << " length: " << arg.len << dec << endl;
      last_syscall_arg = arg.len;
    }
}

/* ========================================================================== */
VOID SyscallExit(THREADID threadIndex, CONTEXT * ictxt, SYSCALL_STANDARD std, VOID *v)
{
    //Single-threaded for now
    assert(threadIndex == 0);

    ADDRINT retval = PIN_GetSyscallReturn(ictxt, std);

    if(last_syscall_number == 90) //oldmap
    {
        ASSERTX( Zesto_Notify_Mmap(retval, last_syscall_arg, false) );
        cout << "Ret syscall oldmmap(" << last_syscall_number << ") addr: 0x" 
             << hex << retval << " length: " << last_syscall_arg << dec << endl;
    }

}

/* ========================================================================== */
INT32 main(INT32 argc, CHAR **argv)
{
    cout << "Command line: ";
    for(int i=0; i<argc; i++)
       cout << argv[i] << " ";
    cout << endl; 

    SSARGS ssargs = MakeSimpleScalarArgcArgv(argc, argv);

    PIN_Init(argc, argv);
    PIN_InitSymbols();

    InstallFastForwarding();

    if(!KnobInsTraceFile.Value().empty())
        trace_file.open(KnobInsTraceFile.Value().c_str());
    trace_file << hex; 

    PIN_AddThreadStartFunction(onMainThreadStart, NULL);  
    IMG_AddUnloadFunction(ImageUnload, 0);
    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_AddSyscallEntryFunction(SyscallEntry, 0);
    PIN_AddSyscallExitFunction(SyscallExit, 0);
    INS_AddInstrumentFunction(Instrument, 0);
    PIN_AddFiniFunction(Fini, 0);

    Zesto_SlaveInit(ssargs.first, ssargs.second);

    PIN_StartProgram();

    return 0;
}
