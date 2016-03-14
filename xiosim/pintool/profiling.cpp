#include <map>
#include <regex>
#include <string>

#include "xiosim/sim.h"
#include "xiosim/synchronization.h"

#include "feeder.h"
#include "BufferManagerProducer.h"
#include "speculation.h"

/* Mark instruction as profiling start point.
 * First in the order of instrumentation, all other routines should adjust. */
static void beforeStart(THREADID tid, ADDRINT pc, UINT32 profile_id) {
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;

    if (!CheckIgnoreConditions(tid, pc))
        return;

    thread_state_t* tstate = get_tls(tid);
    if (speculation_mode) {
        FinishSpeculation(tstate);
        return;
    }

    auto handshake = xiosim::buffer_management::GetBuffer(tstate->tid);
    handshake->flags.is_profiling_start = true;
    handshake->profile_id = profile_id;
}

/* Mark instruction as profiling stop point.
 * First in the order of instrumentation, all other routines should adjust. */
static void beforeStop(THREADID tid, ADDRINT pc, UINT32 profile_id) {
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;

    if (!CheckIgnoreConditions(tid, pc))
        return;

    thread_state_t* tstate = get_tls(tid);
    if (speculation_mode) {
        FinishSpeculation(tstate);
        return;
    }

    auto handshake = xiosim::buffer_management::GetBuffer(tstate->tid);
    handshake->flags.is_profiling_stop = true;
    handshake->profile_id = profile_id;
}

static void MarkInstrumented(ADDRINT pc) {
#ifdef PROFILING_DEBUG
    cerr << "Profiling marker at: " << hex << pc << dec << endl;
#endif
}

static void AddCallback(
        IMG img, std::string sym, ADDRINT offset, AFUNPTR callback, UINT32 profile_id) {
    RTN rtn = RTN_FindByName(img, sym.c_str());
    if (!RTN_Valid(rtn))
        return;

    ADDRINT rtn_start = RTN_Address(rtn);
    RTN_Open(rtn);
    ASSERTX(offset < RTN_Size(rtn));

    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
        ADDRINT pc = INS_Address(ins);
        if (pc != rtn_start + offset)
            continue;

        INS_InsertCall(ins,
                       IPOINT_BEFORE,
                       callback,
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_UINT32,
                       profile_id,
                       IARG_CALL_ORDER,
                       CALL_ORDER_FIRST,
                       IARG_END);
        MarkInstrumented(pc);
    }
    RTN_Close(rtn);
}

static void AddRetCallback(IMG img, std::string sym, AFUNPTR callback, UINT32 profile_id) {
    RTN rtn = RTN_FindByName(img, sym.c_str());
    if (!RTN_Valid(rtn))
        return;

    RTN_Open(rtn);
    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
        if (!INS_IsRet(ins))
            continue;

        INS_InsertCall(ins,
                       IPOINT_BEFORE,
                       callback,
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_UINT32,
                       profile_id,
                       IARG_CALL_ORDER,
                       CALL_ORDER_FIRST,
                       IARG_END);
        ADDRINT pc = INS_Address(ins);
        MarkInstrumented(pc);
    }
    RTN_Close(rtn);
}

/* Parse a start/stop point definition of symbol(+offset).
 * Returns (symbol_name, offset).
 */
static std::pair<std::string, ADDRINT> parse_sym(std::string sym_off) {
    auto re = std::regex("([^\\+]*)(?:\\+)?((?:0x)?[A-Fa-f0-9]+)?");
    std::smatch re_match;

    if (!std::regex_match(sym_off, re_match, re)) {
        std::cerr << "Couldn't parse symbol " << sym_off << std::endl;
        abort();
    }

    if (re_match.size() - 1 != 2) {
        std::cerr << "Knob parse RE has the wrong number of groups." << std::endl;
        abort();
    }

    ADDRINT off = 0;
    if (re_match[2].length() > 0)
        off = std::stol(re_match[2], nullptr, 0);
    return std::make_pair(re_match[1], off);
}

void AddProfilingCallbacks(IMG img) {
    for (size_t i = 0; i < system_knobs.profiling_start.size(); i++) {
        std::string sym_start = system_knobs.profiling_start[i];
        std::string sym_stop =
                i < system_knobs.profiling_stop.size() ? system_knobs.profiling_stop[i] : "";

        if (sym_start == "")
            return;

        auto start_pair = parse_sym(sym_start);
        AddCallback(img, start_pair.first, start_pair.second, AFUNPTR(beforeStart), i);

        if (sym_stop != "") {
            auto stop_pair = parse_sym(sym_stop);
            AddCallback(img, stop_pair.first, stop_pair.second, AFUNPTR(beforeStop), i);
        } else {
            AddRetCallback(img, start_pair.first, AFUNPTR(beforeStop), i);
        }
    }
}
