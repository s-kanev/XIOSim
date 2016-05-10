#include <map>

#include "feeder.h"
#include "ignore_ins.h"
#include "utils.h"

/* TODO(skanev): this file is getting too hard to follow. Clean up! */

struct replacement_info_t {
    UINT32 ins_to_ignore;
    ADDRINT call_alternative_pc;

    replacement_info_t(UINT32 _ins_to_ignore, ADDRINT _call_alternative_pc)
        : ins_to_ignore(_ins_to_ignore)
        , call_alternative_pc(_call_alternative_pc) {}
    replacement_info_t()
        : ins_to_ignore(0)
        , call_alternative_pc(-1) {}
};

static map<ADDRINT, ADDRINT> ignored_tpc;
static XIOSIM_LOCK lk_ignored_tpc;
static map<ADDRINT, replacement_info_t> ignore_ips;

KNOB<BOOL> KnobIgnoringInstructions(KNOB_MODE_WRITEONCE,
                                    "pintool",
                                    "ignore_api",
                                    "false",
                                    "Use ignoring API (usually to replace in-simulator callbacks)");

ADDRINT NextUnignoredPC(ADDRINT pc) {
    ADDRINT curr = pc;
    lk_lock(&lk_ignored_tpc, 1);
    while (ignored_tpc.count(curr)) {
        curr = ignored_tpc[curr];
    }
    lk_unlock(&lk_ignored_tpc);
    return curr;
}

VOID IgnoreCallsTo(ADDRINT addr, UINT32 num_insn, ADDRINT replacement_pc) {
    ignore_ips[addr] = replacement_info_t(num_insn, replacement_pc);
}

VOID IgnorePC(ADDRINT pc, ADDRINT replacement_pc) {
#ifdef IGNORE_DEBUG
    cerr << "Ignoring instruction at 0x" << hex << pc << " repl " << replacement_pc << dec << endl;
#endif
    ignore_ips[pc] = replacement_info_t(1, replacement_pc);
}

static BOOL IsIgnoredInstruction(INS ins) {
    ADDRINT pc = INS_Address(ins);
    return ignore_ips.count(pc) > 0;
}

static ADDRINT InsAlternativePC(INS ins) {
    ADDRINT pc = INS_Address(ins);
    return ignore_ips[pc].call_alternative_pc;
}

static replacement_info_t IsCallToIgnoredFunction(INS ins) {
    ADDRINT target_pc;
    if (INS_IsDirectCall(ins)) {
        target_pc = INS_DirectBranchOrCallTargetAddress(ins);
        if (ignore_ips.count(target_pc))
            return ignore_ips[target_pc];
    }

    return replacement_info_t(0, -1);
}

/* The assumption is that we are never ignoring a jump/branch.
 * Or, that if we are ignoring a call, we are also ignoring the
 * function body, and the function is proper and returns to the
 * falltrough address.
 */
static VOID IgnoreIns(INS ins, ADDRINT alternative_pc = (ADDRINT)-1) {
    ADDRINT pc = INS_Address(ins);
    ADDRINT fallthrough = INS_NextAddress(ins);
    if (alternative_pc != (ADDRINT)-1)
        fallthrough = alternative_pc;

    lk_lock(&lk_ignored_tpc, 1);
    if (ignored_tpc.count(pc) == 0) {
#ifdef IGNORE_DEBUG
        cerr << hex << "Ignoring: " << pc << " ft: " << fallthrough << dec << endl;
#endif
        ignored_tpc[pc] = fallthrough;
    }
    lk_unlock(&lk_ignored_tpc);
}

bool IsInstructionIgnored(ADDRINT pc) {
    if (!KnobIgnoringInstructions.Value())
        return false;

    bool result;
    lk_lock(&lk_ignored_tpc, 1);
    result = (ignored_tpc.count(pc) > 0);
    lk_unlock(&lk_ignored_tpc);
    return result;
}

VOID AddIgnoredInstructionPCs(IMG img, std::vector<std::string>& ignored_pcs) {
    // Support either ignoring instructions with exact PCs or with
    // symbol_name+offset.
    for (std::string sym : ignored_pcs) {
        symbol_off_t symbol_pair;
        ADDRINT pc = 0;
        bool err = !parse_sym(sym, symbol_pair);
        if (err) {
            pc = std::stol(sym, nullptr, 0);
        } else {
            ADDRINT offset = symbol_pair.offset;
            RTN rtn = RTN_FindByName(img, symbol_pair.symbol_name.c_str());
            if (!RTN_Valid(rtn))
                continue;
            ADDRINT rtn_start = RTN_Address(rtn);
            ASSERTX(offset < RTN_Size(rtn));
            pc = rtn_start + offset;
        }
        IgnorePC(pc);
    }
}

VOID InstrumentInsIgnoring(TRACE trace, VOID* v) {
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;

    if (!KnobIgnoringInstructions.Value())
        return;

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            BOOL ignored_ins = IsIgnoredInstruction(ins);
            if (ignored_ins) {
                IgnoreIns(ins, InsAlternativePC(ins));
                continue;
            }

            replacement_info_t repl = IsCallToIgnoredFunction(ins);
            if (repl.ins_to_ignore == 0)
                continue;

            /* Ignoring the caller to a function. */
            IgnoreIns(ins, repl.call_alternative_pc);
            repl.ins_to_ignore--;

            /* Now we go back and ignore X instructions that set up parameters.
             * We assume those are the most recent X stack writes from the same BBL.
             */
            INS curr_ins = ins;
            while (repl.ins_to_ignore > 0) {
                curr_ins = INS_Prev(curr_ins);
                if (!INS_Valid(curr_ins))
                    break;

                if (INS_IsStackWrite(curr_ins)) {
                    IgnoreIns(curr_ins);
                    repl.ins_to_ignore--;
                }
            }
            if (repl.ins_to_ignore > 0) {
                lk_lock(printing_lock, 1);
                cerr << "WARNING: Didn't find enough stack writes to ignore before: " << hex
                     << INS_Address(ins) << dec << endl;
                lk_unlock(printing_lock);
            }
            // assert(repl.ins_to_ignore == 0);
        }
    }
}
