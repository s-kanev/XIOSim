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
#include <sstream>
#include <stdlib.h>
#include <elf.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <syscall.h>
#include <sched.h>
#include <unistd.h>
#include <utility>
#include <signal.h>

#include "boost_interprocess.h"

#include "feeder.h"
#include "multiprocess_shared.h"
#include "ipc_queues.h"
#include "../buffer.h"
#include "BufferManagerProducer.h"
#include "scheduler.h"
#include "ignore_ins.h"

#include "sync_pthreads.h"
#include "syscall_handling.h"
#include "ildjit.h"
#include "parsec.h"

#include "../zesto-core.h"


using namespace std;

/* ========================================================================== */
/* ========================================================================== */
/*                           ZESTO and PIN INTERFACE                          */
/* ========================================================================== */
/* ========================================================================== */

KNOB<string> KnobInsTraceFile(KNOB_MODE_WRITEONCE,   "pintool",
        "trace", "", "File where instruction trace is written");
KNOB<BOOL> KnobILDJIT(KNOB_MODE_WRITEONCE,      "pintool",
        "ildjit", "false", "Application run is ildjit");
KNOB<BOOL> KnobAMDHack(KNOB_MODE_WRITEONCE,      "pintool",
        "amd_hack", "false", "Using AMD syscall hack for use with hpc cluster");
KNOB<string> KnobFluffy(KNOB_MODE_WRITEONCE,      "pintool",
        "fluffy_annotations", "", "Annotation file that specifies fluffy ROI");
KNOB<BOOL> KnobWarmLLC(KNOB_MODE_WRITEONCE,      "pintool",
        "warm_llc", "false", "Warm LLC while fast-forwarding");
KNOB<int> KnobNumCores(KNOB_MODE_WRITEONCE,      "pintool",
        "num_cores", "1", "Number of cores simulated");
KNOB<pid_t> KnobHarnessPid(KNOB_MODE_WRITEONCE, "pintool",
        "harness_pid", "-1", "Process id of the harness process.");

map<ADDRINT, string> pc_diss;

ofstream pc_file;
ofstream trace_file;
ifstream sanity_trace;

// Used to access thread-local storage
static TLS_KEY tls_key;

XIOSIM_LOCK thread_list_lock;
list<THREADID> thread_list;

static inline pid_t gettid() {
    return syscall(SYS_gettid);
}
map<pid_t, THREADID> global_to_local_tid;
XIOSIM_LOCK lk_tid_map;

/* This only matters for a Fini callback where we
 * can't get this information from TLS, which has
 * been destroyed. Ugh. */
static map<THREADID, pid_t> local_to_global_tid;

/* Unique address space id -- the # of this feeder among all */
int asid;

/* ========================================================================== */
/* Pinpoint related */
// Track the number of instructions executed
ICOUNT icount;

// Contains knobs and instrumentation to recognize start/stop points
CONTROL control;

EXECUTION_MODE ExecMode = EXECUTION_MODE_INVALID;
#if 0
map<THREADID, tick_t> lastConsumerApply;
#endif

/* Fallbacks for cases without pinpoints */
INT32 slice_num = 1;
INT32 slice_length = 0;
INT32 slice_weight_times_1000 = 100*1000;

BOOL in_fini = false;

static VOID RegisterSignalIntercept();

static bool producers_sleep;
static PIN_SEMAPHORE producers_sem;
static void wait_producers();

// Functions to access thread-specific data
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
    ipc_message_t msg;
    msg.Munmap(asid, start, length, true);
    SendIPCMessage(msg);
}

/* ========================================================================== */
VOID PPointHandler(CONTROL_EVENT ev, VOID * v, CONTEXT * ctxt, VOID * ip, THREADID tid)
{
    cerr << "tid: " << dec << tid << " ip: " << hex << ip << " ";
    if (tid < ISIMPOINT_MAX_THREADS)
        cerr <<  dec << " Inst. Count " << icount.Count(tid) << " ";

    switch(ev)
    {
      case CONTROL_START:
        {
            cerr << "Start" << endl;
            UINT32 slice_num = 1;
            if(control.PinPointsActive())
                slice_num = control.CurrentPp(tid);

            ipc_message_t msg;
            msg.SliceStart(slice_num);
            SendIPCMessage(msg);

            ExecMode = EXECUTION_MODE_SIMULATE;
            CODECACHE_FlushCache();

            list<THREADID>::iterator it;
            ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
                thread_state_t* tstate = get_tls(*it);
                lk_lock(&tstate->lock, tid+1);
                tstate->firstInstruction = true;
                if (!KnobILDJIT.Value()) {
                    tstate->ignore_all = false;
                    tstate->ignore = false;
                }
                lk_unlock(&tstate->lock);
            }

            // Let caller thread be simulated again
            // XXX: Needs to be done for all threads, and probably not here.
/*            thread_state_t *tstate = get_tls(tid);
            msg.ScheduleNewThread(tstate->tid);
            SendIPCMessage(msg);
*/

            if(control.PinPointsActive())
                cerr << "PinPoint: " << control.CurrentPp(tid) << " PhaseNo: " << control.CurrentPhase(tid) << endl;
        }
        break;

      case CONTROL_STOP:
        {
            /* XXX: This can be called from a Fini callback (end of program).
             * We can't access any TLS in that case. */
            if (!in_fini) {
                list<THREADID>::iterator it;
                ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
                    thread_state_t* tstate = get_tls(*it);
                    lk_lock(&tstate->lock, tid+1);
                    tstate->ignore_all = true;
                    lk_unlock(&tstate->lock);
                }

                /* In this case, we need to flush all buffers for this thread
                 * and cleanly let the scheduler know to deschedule it once
                 * all instructions are conusmed. */
                pid_t global_tid;
                if (in_fini)
                    global_tid = local_to_global_tid[tid];
                else {
                    global_tid = get_tls(tid)->tid;
                }

                handshake_container_t *handshake = xiosim::buffer_management::get_buffer(global_tid);
                handshake->flags.giveCoreUp = true;
                handshake->flags.giveUpReschedule = false;
                handshake->flags.valid = true;
                handshake->handshake.real = false;
                xiosim::buffer_management::producer_done(global_tid, true);

                xiosim::buffer_management::flushBuffers(global_tid);
            }

            lk_lock(printing_lock, 1);
            cerr << "Stop" << endl;
            lk_unlock(printing_lock);

            PauseSimulation();

            /* Here, nothing guarantees that there is still a thread with an active
             * handshake buffer (this handler can be called from a Fini callback
             * after all threads have exited).
             * Returning from PauseSimulation() guarantees that all sim threads
             * are spinning in SimulatorLoop. So we can safely call Slice_End
             * without racing any of them. */
/*
            ipc_message_t msg;
            if(control.PinPointsActive())
            {
                cerr << "PinPoint: " << control.CurrentPp(tid) << endl;
                msg.SliceEnd(control.CurrentPp(tid), control.CurrentPpLength(tid), control.CurrentPpWeightTimesThousand(tid));
            }
            else {
                msg.SliceEnd(slice_num, slice_length, slice_weight_times_1000);
            }
            SendIPCMessage(msg);
*/
            ExecMode = EXECUTION_MODE_FASTFORWARD;
            CODECACHE_FlushCache();
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

    // Register callback interface to get notified on ILDJIT events
    if (KnobILDJIT.Value())
        AddILDJITCallbacks(img);

    if (KnobPthreads.Value())
        AddPthreadsCallbacks(img);

    if (KnobParsec.Value())
        AddParsecCallbacks(img);

    ipc_message_t msg;
    msg.Mmap(asid, start, length, false);
    SendIPCMessage(msg);
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
    ssregs->regs_C.aflags = PIN_GetContextReg(&ssctxt, LEVEL_BASE::REG_EFLAGS);
    ssregs->regs_R.dw[MD_REG_EAX] = PIN_GetContextReg(&ssctxt, LEVEL_BASE::REG_EAX);
    ssregs->regs_R.dw[MD_REG_ECX] = PIN_GetContextReg(&ssctxt, LEVEL_BASE::REG_ECX);
    ssregs->regs_R.dw[MD_REG_EDX] = PIN_GetContextReg(&ssctxt, LEVEL_BASE::REG_EDX);
    ssregs->regs_R.dw[MD_REG_EBX] = PIN_GetContextReg(&ssctxt, LEVEL_BASE::REG_EBX);
    ssregs->regs_R.dw[MD_REG_ESP] = PIN_GetContextReg(&ssctxt, LEVEL_BASE::REG_ESP);
    ssregs->regs_R.dw[MD_REG_EBP] = PIN_GetContextReg(&ssctxt, LEVEL_BASE::REG_EBP);
    ssregs->regs_R.dw[MD_REG_EDI] = PIN_GetContextReg(&ssctxt, LEVEL_BASE::REG_EDI);
    ssregs->regs_R.dw[MD_REG_ESI] = PIN_GetContextReg(&ssctxt, LEVEL_BASE::REG_ESI);


    // Copy segment selector registers (IA32-specific)
    ssregs->regs_S.w[MD_REG_CS] = PIN_GetContextReg(&ssctxt, LEVEL_BASE::REG_SEG_CS);
    ssregs->regs_S.w[MD_REG_SS] = PIN_GetContextReg(&ssctxt, LEVEL_BASE::REG_SEG_SS);
    ssregs->regs_S.w[MD_REG_DS] = PIN_GetContextReg(&ssctxt, LEVEL_BASE::REG_SEG_DS);
    ssregs->regs_S.w[MD_REG_ES] = PIN_GetContextReg(&ssctxt, LEVEL_BASE::REG_SEG_ES);
    ssregs->regs_S.w[MD_REG_FS] = PIN_GetContextReg(&ssctxt, LEVEL_BASE::REG_SEG_FS);
    ssregs->regs_S.w[MD_REG_GS] = PIN_GetContextReg(&ssctxt, LEVEL_BASE::REG_SEG_GS);

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
// Register that we are about to service Fini callbacks, which cannot
// access some state (e.g. TLS)
VOID BeforeFini(INT32 exitCode, VOID *v)
{
    in_fini = true;

    /* Make sure we have a mappig to global tid, which we'll
     * need to clean stuff up. */
    list<THREADID>::iterator it;
    ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
        auto curr_tstate = get_tls(*it);
        local_to_global_tid[*it] = curr_tstate->tid;
    }
}

/* ========================================================================== */
VOID Fini(INT32 exitCode, VOID *v)
{
    if (exitCode != EXIT_SUCCESS)
        cerr << "[" << getpid() << "]" << "ERROR! Exit code = " << dec << exitCode << endl;
}

/* ========================================================================== */
VOID MakeSSRequest(THREADID tid, ADDRINT pc, ADDRINT npc, ADDRINT tpc, BOOL brtaken, const CONTEXT *ictxt, handshake_container_t* hshake)
{
    thread_state_t* tstate = get_tls(tid);
    MakeSSContext(ictxt, &tstate->fpstate_buf, pc, npc, &hshake->handshake.ctxt);

    hshake->handshake.asid = asid;
    hshake->handshake.pc = pc;
    hshake->handshake.npc = npc;
    hshake->handshake.tpc = tpc;
    hshake->handshake.brtaken = brtaken;
    hshake->handshake.sleep_thread = FALSE;
    hshake->handshake.resume_thread = FALSE;
    hshake->handshake.real = TRUE;
    PIN_SafeCopy(hshake->handshake.ins, (VOID*) pc, MD_MAX_ILEN);
}

/* Helper to check if producer thread @tid will grab instruction at @pc.
 * If return value is false, we can skip instrumentaion. Can hog execution
 * if producers are disabled by producer_sleep. */
static BOOL CheckIgnoreConditions(THREADID tid, ADDRINT pc)
{
    wait_producers();

    /* Check thread ignore and ignore_all flags */
    thread_state_t* tstate = get_tls(tid);
    lk_lock(&tstate->lock, tid+1);
    if (tstate->ignore || tstate->ignore_all) {
        lk_unlock(&tstate->lock);
        return false;
    }
    lk_unlock(&tstate->lock);

    /* Check ignore API for unnecessary instructions */
    if (IsInstructionIgnored(pc))
        return false;

    return true;
}

/* Helper to grab the correct buffer that we put instrumentation information
 * for thread @tid. Either a new one (@first_instrumentation == true), or the
 * last one being filled in. */
static handshake_container_t* GetProperBuffer(pid_t tid, BOOL first_instrumentation)
{
    handshake_container_t* res;
    if (first_instrumentation) {
        res = xiosim::buffer_management::get_buffer(tid);
    }
    else {
        res = xiosim::buffer_management::back(tid);
    }
    ASSERTX(res != NULL);
    ASSERTX(!res->flags.valid);
    return res;
}

/* Helper to mark the buffer of a fully instrumented instruction as valid,
 * and let it get eventually consumed. */
static VOID FinalizeBuffer(thread_state_t* tstate, handshake_container_t* handshake)
{
    // Let simulator consume instruction from SimulatorLoop
    handshake->flags.valid = true;
    xiosim::buffer_management::producer_done(tstate->tid);

    if (tstate->num_inst && (tstate->num_inst % 1000000 == 0))
        xiosim::buffer_management::flushBuffers(tstate->tid);
}

/* ========================================================================== */
/* We grab the memory location that an instruction touches BEFORE executing the
 * instructions. For reads, this is what the read will return. For writes, this
 * is the value that will get overwritten (which we need later in the simulator
 * to clean up corner cases of speculation */
VOID GrabInstructionMemory(THREADID tid, ADDRINT addr, UINT32 size, BOOL first_mem_op, ADDRINT pc)
{
    if (!CheckIgnoreConditions(tid, pc))
        return;

    thread_state_t* tstate = get_tls(tid);
    handshake_container_t* handshake = GetProperBuffer(tstate->tid, first_mem_op);

    /* should be the common case */
    if (first_mem_op && size <= 4) {
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
VOID GrabInstructionContext(THREADID tid, ADDRINT pc, BOOL taken, ADDRINT npc, ADDRINT tpc, const CONTEXT *ictxt, BOOL has_memory, BOOL done_instrumenting)
{
    if (!CheckIgnoreConditions(tid, pc)) {
#ifdef PRINT_DYN_TRACE
        printTrace("jit", pc, tid);
#endif
        return;
    }

#ifdef PRINT_DYN_TRACE
    printTrace("sim", pc, tid);
#endif

    thread_state_t* tstate = get_tls(tid);
    handshake_container_t* handshake = GetProperBuffer(tstate->tid, !has_memory);

    tstate->num_inst++;

    lk_lock(&tstate->lock, tid+1);
    BOOL first_insn = tstate->firstInstruction;
    lk_unlock(&tstate->lock);
    if (first_insn) {
        handshake->flags.isFirstInsn = true;
        lk_lock(&tstate->lock, tid+1);
        tstate->firstInstruction = false;
        lk_unlock(&tstate->lock);
    }

    // Populate handshake buffer
    MakeSSRequest(tid, pc, NextUnignoredPC(npc), NextUnignoredPC(tpc), taken, ictxt, handshake);

    // If no more steps, instruction is ready to be consumed
    if (done_instrumenting)
        FinalizeBuffer(tstate, handshake);
}

/* ========================================================================== */
/* If we treat REP instructions as loops and pass them along to the simulator,
 * we need a good ground truth for the NPC that the simulator can rely on, because
 * Pin doesn't do that for us the way does branch NPCs.
 * So, we add extra instrumentation for REP instructions to determine if this is
 * the last iteration. */
VOID FixRepInstructionNPC(THREADID tid, ADDRINT pc, BOOL rep_prefix, BOOL repne_prefix, ADDRINT counter_value, UINT32 opcode)
{
    if (!CheckIgnoreConditions(tid, pc))
        return;

    thread_state_t* tstate = get_tls(tid);
    handshake_container_t* handshake = GetProperBuffer(tstate->tid, false);

    BOOL scan = false, zf = false;
    ADDRINT op2 = 0;

    // REPE and REPNE only matter for CMPS and SCAS,
    // so we special-case them
    switch (opcode) {
        case XED_ICLASS_CMPSB:
        case XED_ICLASS_CMPSW:
        case XED_ICLASS_CMPSD:
        case XED_ICLASS_CMPSQ:
            // CMPS does two mem reads, second one is already stored in mem_buffer
            {
                ASSERTX(handshake->mem_buffer.size() <= 4);
                int i=0;
                for (auto it = handshake->mem_buffer.begin(); it != handshake->mem_buffer.end(); it++, i++)
                    op2 |= ((ADDRINT)it->second << (8*i));

            }
            scan = true;
            zf = (op2 == handshake->handshake.mem_val);
            break;
        case XED_ICLASS_SCASB:
        case XED_ICLASS_SCASW:
        case XED_ICLASS_SCASD:
        case XED_ICLASS_SCASQ:
            // SCAS only does one read, gets second operand from rAX
            ASSERTX(handshake->mem_buffer.size() <= 4);
            op2 = handshake->handshake.ctxt.regs_R.dw[MD_REG_EAX]; // 0-extended anyways
            scan = true;
            zf = (op2 == handshake->handshake.mem_val);
            break;
        case XED_ICLASS_INSB:
        case XED_ICLASS_INSW:
        case XED_ICLASS_INSD:
        case XED_ICLASS_OUTSB:
        case XED_ICLASS_OUTSW:
        case XED_ICLASS_OUTSD:
        case XED_ICLASS_LODSB:
        case XED_ICLASS_LODSW:
        case XED_ICLASS_LODSD:
        case XED_ICLASS_LODSQ:
        case XED_ICLASS_STOSB:
        case XED_ICLASS_STOSW:
        case XED_ICLASS_STOSD:
        case XED_ICLASS_STOSQ:
        case XED_ICLASS_MOVSB:
        case XED_ICLASS_MOVSW:
        case XED_ICLASS_MOVSD:
        case XED_ICLASS_MOVSQ:
            scan = false;
            break;
        default:
            ASSERTX(false);
            break;
    }

    ADDRINT NPC;
    // Counter says we finish after this instruction (for all prefixes)
    if (counter_value == 1 || counter_value == 0)
        NPC = handshake->handshake.tpc;
    // Zero flag and REPE/REPNE prefixes say we finish after this instruction
    else if (scan && ((repne_prefix && zf) || (rep_prefix && !zf)))
        NPC = handshake->handshake.tpc;
    // Otherwise we just keep looping
    else
        NPC = handshake->handshake.pc;

    // Update with hard-earned NPC
    handshake->handshake.npc = NPC;
    handshake->handshake.ctxt.regs_NPC = NPC;

    // Instruction is ready to be consumed
    FinalizeBuffer(tstate, handshake);
}

/* ========================================================================== */
//Trivial call to let us do conditional instrumentation based on an argument
ADDRINT returnArg(BOOL arg)
{
   return arg;
}

VOID WarmCacheRead(VOID * addr)
{
#if 0
    Zesto_WarmLLC((ADDRINT)addr, false);
#endif
}

VOID WarmCacheWrite(VOID * addr)
{
#if 0
    Zesto_WarmLLC((ADDRINT)addr, true);
#endif
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
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
        UINT32 memSize = INS_MemoryOperandSize(ins, memOp);
        INS_InsertCall(
            ins, IPOINT_BEFORE, (AFUNPTR)GrabInstructionMemory,
            IARG_THREAD_ID,
            IARG_MEMORYOP_EA, memOp,
            IARG_UINT32, memSize,
            IARG_BOOL, (memOp == 0),
            IARG_INST_PTR,
            IARG_END);
    }

    if (! INS_IsBranchOrCall(ins))
    {
        BOOL extraRepInstrumentation = INS_HasRealRep(ins);
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) GrabInstructionContext,
                   IARG_THREAD_ID,
                   IARG_INST_PTR,
                   IARG_BOOL, 0,
                   IARG_ADDRINT, INS_NextAddress(ins),
                   IARG_FALLTHROUGH_ADDR,
                   IARG_CONST_CONTEXT,
                   IARG_BOOL, (memOperands > 0),
                   IARG_BOOL, !extraRepInstrumentation,
                   IARG_END);

        if(extraRepInstrumentation)
           INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) FixRepInstructionNPC,
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_BOOL, INS_RepPrefix(ins),
                       IARG_BOOL, INS_RepnePrefix(ins),
                       IARG_REG_VALUE, INS_RepCountRegister(ins),
                       IARG_ADDRINT, INS_Opcode(ins),
                       IARG_END);
    }
    else
    {
        // Branch, give instrumentation appropriate address
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) GrabInstructionContext,
                   IARG_THREAD_ID,
                   IARG_INST_PTR,
                   IARG_BRANCH_TAKEN,
                   IARG_ADDRINT, INS_NextAddress(ins),
                   IARG_BRANCH_TARGET_ADDR,
                   IARG_CONST_CONTEXT,
                   IARG_BOOL, (memOperands > 0),
                   IARG_BOOL, true,
                   IARG_END);
    }
}

/* ========================================================================== */
VOID ThreadStart(THREADID threadIndex, CONTEXT * ictxt, INT32 flags, VOID *v)
{
    // ILDJIT is forking a compiler thread, ignore
//    if (KnobILDJIT.Value() && !ILDJIT_IsCreatingExecutor())
//        return;

    lk_lock(&syscall_lock, 1);

    thread_state_t* tstate = new thread_state_t(threadIndex);
    PIN_SetThreadData(tls_key, tstate, threadIndex);

    ADDRINT tos, bos;

    tos = PIN_GetContextReg(ictxt, LEVEL_BASE::REG_ESP);
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
                ipc_message_t msg;
                msg.Mmap(asid, vsyscall_page, PAGE_SIZE, false);
                SendIPCMessage(msg);
            }
        }

        if (last_env != NULL)
            bos = (ADDRINT) last_env + strlen(last_env)+1;
        else
            bos = (ADDRINT) last_argv + strlen(last_argv)+1; //last_argv != NULLalways
//        cerr << "bos: " << hex << bos << dec << endl;

        // Reserve space for environment and arguments in case
        // execution starts on another thread.
        ADDRINT tos_start = ROUND_DOWN(tos, PAGE_SIZE);
        ADDRINT bos_end = ROUND_UP(bos, PAGE_SIZE);
        ipc_message_t msg;
        msg.Mmap(asid, tos_start, bos_end-tos_start, false);
        SendIPCMessage(msg);

    }
    else {
        bos = tos;
    }

    // Application threads only -- create buffers for them
    if (!KnobILDJIT.Value() ||
        (KnobILDJIT.Value() && ILDJIT_IsCreatingExecutor()))
    {
        /* Store globally unique thread id */
        tstate->tid = gettid();

        // Create new buffer to store thread context
        xiosim::buffer_management::AllocateThreadProducer(tstate->tid);

#if 0
        lastConsumerApply[threadIndex] = 0;
#endif
        lk_lock(&lk_tid_map, 1);
        global_to_local_tid[tstate->tid] = threadIndex;
        lk_unlock(&lk_tid_map);

        // Mark thread as belonging to this process
        lk_lock(lk_threadProcess, 1);
        threadProcess->operator[](tstate->tid) = asid;
        lk_unlock(lk_threadProcess);

        // Remember bottom of stack for this thread globally
        lk_lock(lk_thread_bos, 1);
        thread_bos->operator[](tstate->tid) = bos;
        lk_unlock(lk_thread_bos);

        if (ExecMode == EXECUTION_MODE_SIMULATE)
        {
            ipc_message_t msg;
            msg.ScheduleNewThread(tstate->tid);
            SendIPCMessage(msg);
        }

        if (!KnobILDJIT.Value())
        {
            lk_lock(&tstate->lock, threadIndex+1);
            tstate->ignore = false;
            tstate->ignore_all = false;
            lk_unlock(&tstate->lock);
        }

        /* Add to thread_list in the end, so other threads that iterate
         * it don't race on a not yet fully initialized structure.
         */
        lk_lock(&thread_list_lock, threadIndex+1);
        thread_list.push_back(threadIndex);
        lk_unlock(&thread_list_lock);
    }

    lk_unlock(&syscall_lock);
}

/* ========================================================================== */
/* Make sure that all sim threads drain any handshake buffers that could be in
 * their respective scheduler run queues.
 * Invariant: after this call, all sim threads are spinning in SimulatorLoop */
VOID PauseSimulation()
{
    /* Here we have produced everything for this loop! */
    disable_producers();
    enable_consumers();

    /* Since the scheduler is updated from the sim threads in SimulatorLoop,
     * seeing empty run queues is enough to determine that the sim thread
     * is done simulating instructions. No need for fancier synchronization. */
    bool threads_done = true;
    do {
        threads_done = true;
        list<THREADID>::iterator it;
        ATOMIC_ITERATE(thread_list, it, thread_list_lock) {
            thread_state_t* tstate = get_tls(*it);
            threads_done &= !IsSHMThreadSimulatingMaybe(tstate->tid);
        }
    } while (!threads_done);
}

/* ========================================================================== */
VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
    lk_lock(printing_lock, tid+1);
    cerr << "Thread exit. ID: " << tid << endl;
    lk_unlock(printing_lock);

    thread_state_t* tstate = get_tls(tid);

    BOOL was_scheduled = tstate->num_inst > 0;

    /* Ignore threads which we weren't going to simulate.
       It's ok that we're not locking tstate here, because no one is using it */
    if (!was_scheduled) {
        delete tstate;
        PIN_DeleteThreadDataKey(tid);
        return;
    }

    /* There will be no further instructions instrumented (on this thread).
     * Mark it as finishing and let the handshake buffer drain.
     * Once this last handshake gets executed by a core, it will make
       sure to clean up all thread resources. */
    handshake_container_t *handshake = xiosim::buffer_management::get_buffer(tstate->tid);
    handshake->flags.killThread = true;
    handshake->flags.valid = true;
    handshake->handshake.real = false;
    xiosim::buffer_management::producer_done(tstate->tid);

    xiosim::buffer_management::flushBuffers(tstate->tid);

    /* Ignore subsequent instructions that we may see on this thread before
     * destroying its tstate.
     * XXX: This might be bit paranoid depending on when Pin inserts the
     * ThreadFini callback. */
    lk_lock(&tstate->lock, tid+1);
    tstate->ignore = true;
    lk_unlock(&tstate->lock);
}

/* ========================================================================== */
INT32 main(INT32 argc, CHAR **argv)
{
#ifdef ZESTO_PIN_DBG
    cerr << "[" << getpid() << "]" << " feeder_zesto args: ";
    for(int i=0; i<argc; i++)
       cerr << argv[i] << " ";
    cerr << endl;
#endif

    // Obtain  a key for TLS storage.
    tls_key = PIN_CreateThreadDataKey(0);

    PIN_Init(argc, argv);
    PIN_InitSymbols();

    RegisterSignalIntercept();
    PIN_SemaphoreInit(&producers_sem);

    // Synchronize all processes here to ensure that in multiprogramming mode,
    // no process will start too far before the others.
    asid = InitSharedState(true, KnobHarnessPid.Value(), KnobNumCores.Value());
    xiosim::buffer_management::InitBufferManagerProducer(KnobHarnessPid.Value(), KnobNumCores.Value());

    if(KnobAMDHack.Value()) {
        amd_hack();
    }

    if (KnobILDJIT.Value()) {
        MOLECOOL_Init();
    }

    if (!KnobILDJIT.Value() && !KnobParsec.Value()) {
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

    // Delay this instrumentation until startSimulation call in ILDJIT.
    // This cuts down HELIX compilation noticably for integer benchmarks.

    if(!KnobILDJIT.Value()) {
        TRACE_AddInstrumentFunction(InstrumentInsIgnoring, 0);
        INS_AddInstrumentFunction(Instrument, 0);
    }

    PIN_AddThreadStartFunction(ThreadStart, NULL);
    PIN_AddThreadFiniFunction(ThreadFini, NULL);
//    IMG_AddUnloadFunction(ImageUnload, 0);
    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_AddFiniUnlockedFunction(BeforeFini, 0);
    PIN_AddFiniFunction(Fini, 0);
    InitSyscallHandling();

    *sleeping_enabled = true;
    enable_producers();

    PIN_StartProgram();

    return 0;
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
  TRACE_AddInstrumentFunction(InstrumentInsIgnoring, 0);
  INS_AddInstrumentFunction(Instrument, 0);
  CODECACHE_FlushCache();
  PIN_UnlockClient();

  calledAlready = true;
}

VOID printTrace(string stype, ADDRINT pc, pid_t tid)
{
  if(ExecMode != EXECUTION_MODE_SIMULATE) {
    return;
  }

  lk_lock(printing_lock, tid+1);
  pc_file << tid << " " << stype << " " << pc << " " << pc_diss[pc] << endl;
  pc_file.flush();
  lk_unlock(printing_lock);
}

/* Explicitly print signal info. Under some conditions, it does miss.
 * This makes debugging segfaults easier. */
static BOOL SignalInfo(THREADID tid, INT32 sig, CONTEXT *ctxt, BOOL hasHandler, const EXCEPTION_INFO *pExceptInfo, VOID *v)
{
    cerr << "Caught signal " <<  sig << " at " << hex << PIN_GetExceptionAddress(pExceptInfo) << dec << endl;
    cerr << PIN_ExceptionToString(pExceptInfo) << endl;

    return true;
}

static VOID RegisterSignalIntercept()
{
    PIN_InterceptSignal(SIGINT, SignalInfo, NULL);
    PIN_InterceptSignal(SIGABRT, SignalInfo, NULL);
    PIN_InterceptSignal(SIGFPE, SignalInfo, NULL);
    PIN_InterceptSignal(SIGILL, SignalInfo, NULL);
    PIN_InterceptSignal(SIGSEGV, SignalInfo, NULL);
    PIN_InterceptSignal(SIGTERM, SignalInfo, NULL);
    PIN_InterceptSignal(SIGKILL, SignalInfo, NULL);
}

void disable_producers()
{
    if (*sleeping_enabled) {
        if (!producers_sleep)
            PIN_SemaphoreClear(&producers_sem);
        producers_sleep = true;
    }
}

void enable_producers()
{
    if (producers_sleep)
        PIN_SemaphoreSet(&producers_sem);
    producers_sleep = false;
}

static void wait_producers()
{
    if (!*sleeping_enabled)
        return;

    if (producers_sleep)
        PIN_SemaphoreWait(&producers_sem);
}
