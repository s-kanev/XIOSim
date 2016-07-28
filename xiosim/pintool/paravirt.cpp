#include <syscall.h>

#include "xiosim/core_const.h"
#include "xiosim/synchronization.h"

#include "feeder.h"
#include "multiprocess_shared.h"
#include "speculation.h"
#include "vdso.h"
#include "paravirt.h"

KNOB<BOOL> KnobTimingVirtualization(
        KNOB_MODE_WRITEONCE, "pintool", "timing_virtualization", "true",
        "Return simulated time instead of host time for timing calls like gettimeofday().");

static XIOSIM_LOCK vdso_lock;
static ADDRINT gettimeofday_addr = 0;
static size_t gettimeofday_size = 0;

static void FindVDSORoutines() {
    std::lock_guard<XIOSIM_LOCK> l(vdso_lock);

    uintptr_t vdso = vdso_addr();
    vdso_init_from_sysinfo_ehdr(vdso);
    std::tie(gettimeofday_addr, gettimeofday_size) = vdso_sym("__vdso_gettimeofday");
}

/* If we come here theough the VDSO route, no locks are held. */
static XIOSIM_LOCK initial_time_lock;
// These fields will be populated by the first call to gettimeofday().
static struct timeval initial_system_time = { 0, 0 };
static double initial_global_sim_time = 0;

void BeforeGettimeofday(THREADID tid, ADDRINT arg1) {
    if (!KnobTimingVirtualization.Value())
        return;

    thread_state_t* tstate = get_tls(tid);
    if (speculation_mode) {
        FinishSpeculation(tstate);
        return;
    }

    tstate->last_syscall_number = __NR_gettimeofday;
    tstate->last_syscall_arg1 = arg1;
}

static void BeforeGettimeofdayIA32Wrapper(THREADID tid, ADDRINT esp) {
    ADDRINT arg1;
    VOID* arg1_addr = (VOID*)(esp - 0x4);
    PIN_SafeCopy(&arg1, arg1_addr, sizeof(ADDRINT));

    BeforeGettimeofday(tid, arg1);
}

/* Virtualization of gettimeofday which returns the value of simulated global time */
void AfterGettimeofday(THREADID tid, ADDRINT retval) {
    if (!KnobTimingVirtualization.Value())
        return;

    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;

    thread_state_t* tstate = get_tls(tid);
    if (speculation_mode) {
        FinishSpeculation(tstate);
        return;
    }

    if (retval == (ADDRINT)-1)
        return;

#ifdef SYSCALL_DEBUG
    std::cerr << "[" << tstate->tid << "]" << "gettimeofday will wait." << std::endl;
#endif

    timeval* tv = (struct timeval*)tstate->last_syscall_arg1;
    SyncWithTimingSim(tid);
    {
        std::lock_guard<XIOSIM_LOCK> l(initial_time_lock);
        if (initial_system_time.tv_sec == 0 && initial_system_time.tv_usec == 0) {
            // If this is the first time we're calling gettimeofday(),
            // return the host time and use this as the offset for all
            // future calls (which will return simulated time).
            initial_system_time.tv_sec = tv->tv_sec;
            initial_system_time.tv_usec = tv->tv_usec;
            // Record the initial global_sim_time as computed by the
            // timing simulator for computing the delta.
            initial_global_sim_time = *global_sim_time;
        } else {
            double sim_time_passed = *global_sim_time - initial_global_sim_time;
            time_t secs_passed = (sim_time_passed / 1000000.0);
            suseconds_t usecs_passed = sim_time_passed - secs_passed * 1000000.0;
            tv->tv_sec = secs_passed + initial_system_time.tv_sec;
            tv->tv_usec = usecs_passed + initial_system_time.tv_usec;
        }
    }
    ScheduleThread(tid);

    tstate->last_syscall_number = 0;

#ifdef SYSCALL_DEBUG
    std::cerr << "[" << tstate->tid << "]" << "gettimeofday waited." << std::endl;
#endif
}

/* Virtualization of rdtsc which returns the value of sim_cycle for the core. */
VOID ReadRDTSC(THREADID tid, ADDRINT pc, ADDRINT next_pc, CONTEXT *ictxt) {
    if (!CheckIgnoreConditions(tid, pc))
        return;

    thread_state_t* tstate = get_tls(tid);
    if (speculation_mode) {
        FinishSpeculation(tstate);
        return;
    }

    if (tstate->last_syscall_number == __NR_gettimeofday) {
        return;
    }

#ifdef SYSCALL_DEBUG
    std::cerr << "[" << tstate->tid << "]" << "RDTSC will wait." << std::endl;
#endif

    // We need to get the core this thread is running on before we sync
    // (which requires descheduling said thread).
    int core_id = GetSHMThreadCore(tstate->tid);
    SyncWithTimingSim(tid);
    tick_t sim_cycles = 0;
    uint32_t lo = 0, hi = 0;
    if (core_id != xiosim::INVALID_CORE)
        sim_cycles = timestamp_counters[core_id];
    tick_t current_timestamp = initial_timestamps[core_id] + sim_cycles;
    lo = current_timestamp & 0xFFFFFFFF;
    hi = (current_timestamp >> 32);

    PIN_SetContextRegval(ictxt, LEVEL_BASE::REG_EAX, reinterpret_cast<UINT8*>(&lo));
    PIN_SetContextRegval(ictxt, LEVEL_BASE::REG_EDX, reinterpret_cast<UINT8*>(&hi));
    PIN_SetContextRegval(ictxt, LEVEL_BASE::REG_INST_PTR, reinterpret_cast<UINT8*>(&next_pc));

    ScheduleThread(tid);

#ifdef SYSCALL_DEBUG
    std::cerr << "[" << tstate->tid << "]" << "RDTSC waited." << std::endl;
#endif
    PIN_ExecuteAt(ictxt);
}

VOID InstrumentParavirt(TRACE trace, VOID* v) {
    if (!KnobTimingVirtualization.Value())
        return;

    if (gettimeofday_addr == 0) {
        FindVDSORoutines();
    }

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {

            /* RDTSC virtualization. */
            if (INS_Opcode(ins) == XED_ICLASS_RDTSC ||
                INS_Opcode(ins) == XED_ICLASS_RDTSCP) {
                INS_InsertCall(ins,
                               IPOINT_BEFORE,
                               (AFUNPTR)ReadRDTSC,
                               IARG_THREAD_ID,
                               IARG_INST_PTR,
                               IARG_ADDRINT,
                               INS_NextAddress(ins),
                               IARG_CONTEXT,
                               IARG_END);
            }

            /* We have to do some extra work for __vdso_gettimeofday().
             * Pin doesn't expose the VDSO through the IMG APIs (but lets us instrument
             * instructions that belong in it).
             * So, we'll manually parse the ELF symbols for it, and instrument the
             * first instruction and all ret-s.
             * It's a little uglier than doing RTN instrumentation, but we can't add
             * RTN instrumentation from a TRACE or INS routine.
             */
            ADDRINT pc = INS_Address(ins);
            if (pc == gettimeofday_addr) {
#ifdef SYSCALL_DEBUG
                std::cerr << "Found __vdso_gettimeofday at " << std::hex << pc << std::dec
                          << std::endl;
#endif

#ifdef _LP64
                INS_InsertCall(ins,
                               IPOINT_BEFORE,
                               AFUNPTR(BeforeGettimeofday),
                               IARG_THREAD_ID,
                               IARG_REG_VALUE,
                               LEVEL_BASE::REG_FullRegName(LEVEL_BASE::REG_EDI),
                               IARG_END);
#else
                INS_InsertCall(ins,
                               IPOINT_BEFORE,
                               AFUNPTR(BeforeGettimeofdayIA32Wrapper),
                               IARG_THREAD_ID,
                               IARG_REG_VALUE,
                               LEVEL_BASE::REG_FullRegName(LEVEL_BASE::REG_ESP),
                               IARG_END);
#endif
            }

            if (INS_IsRet(ins) && pc >= gettimeofday_addr &&
                pc < gettimeofday_addr + gettimeofday_size) {
#ifdef SYSCALL_DEBUG
                std::cerr << "Found __vdso_gettimeofday ret at " << std::hex << pc << std::dec
                          << std::endl;
#endif
                INS_InsertCall(ins,
                               IPOINT_BEFORE,  // Pin can't do after for a ret
                               AFUNPTR(AfterGettimeofday),
                               IARG_THREAD_ID,
                               IARG_REG_VALUE,
                               LEVEL_BASE::REG_FullRegName(LEVEL_BASE::REG_EAX),
                               IARG_END);
            }
        }
    }
}
