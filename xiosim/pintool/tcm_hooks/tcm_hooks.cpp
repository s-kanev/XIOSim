#include <cctype>
#include <iomanip>
#include <iostream>
#include <string>

#include "xiosim/knobs.h"
#include "xiosim/decode.h"
#include "xiosim/size_class_cache.h"
#include "xiosim/regs.h"

#include "xiosim/pintool/BufferManagerProducer.h"
#include "xiosim/pintool/feeder.h"
#include "xiosim/pintool/replace_function.h"
#include "xiosim/pintool/xed_utils.h"

#include "tcm_hooks.h"
#include "tcm_opts.h"
#include "tcm_utils.h"

KNOB<BOOL> KnobTCMHooks(KNOB_MODE_WRITEONCE, "pintool", "tcm_hooks", "true",
                        "Emulate tcmalloc replacements.");
KNOB<std::string> KnobFreelistMode(
        KNOB_MODE_WRITEONCE, "pintool", "freelist_mode", "baseline",
        "Simulate the magic freelist instructions with either zero latency (ideal), "
        "a realistic accelerator implementation (realistic), or the baseline "
        "implementation (baseline).");
KNOB<std::string> KnobSizeClassMode(
        KNOB_MODE_WRITEONCE, "pintool", "size_class_mode", "baseline",
        "Desired mode for simulating the magic blsr and shld instructions.");
KNOB<std::string> KnobSamplingMode(
        KNOB_MODE_WRITEONCE, "pintool", "sampling_mode", "baseline",
        "Desired mode for simulating the magic adc sequence.");


extern MagicInsMode freelist_mode;
extern MagicInsMode size_class_mode;
extern MagicInsMode sampling_mode;

static void MarkMagicInstruction(THREADID tid, ADDRINT pc) {
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;

    if (!CheckIgnoreConditions(tid, pc))
        return;

    thread_state_t* tstate = get_tls(tid);
    auto handshake = xiosim::buffer_management::GetBuffer(tstate->tid);
    /* Mark this handshake, so the regular instrumentation knows someone's already
     * tampered with it. */
    handshake->pc = pc;
    handshake->flags.real = false;
}

/* Helper to insert a call to MarkMagicInstruction() on an instruction. */
static void MarkMagicInstructionHelper(INS ins) {
    /* When we're simulating, mark the placeholder instruction as magic, so the
     * timing simulator can evaluate performance. */
    INS_InsertCall(ins,
                   IPOINT_BEFORE,
                   AFUNPTR(MarkMagicInstruction),
                   IARG_THREAD_ID,
                   IARG_INST_PTR,
                   IARG_CALL_ORDER,
                   CALL_ORDER_FIRST + 2,
                   IARG_END);
}

static void InsertEmulationCode(INS ins, xed_iclass_enum_t iclass) {
#ifdef TCM_DEBUG
    std::cerr << "Inserting emulation code";
#endif
    /* Add emulation regardless of whether we're simulating or not, so we
     * can preserve program semantics. */
    switch (iclass) {
    case XED_ICLASS_LZCNT: {
#ifdef TCM_DEBUG
        std::cerr << " for lzcnt (SLL_Pop).";
#endif
        SLLPop::RegisterEmulation(ins);
        break;
    }
    case XED_ICLASS_BEXTR: {
#ifdef TCM_DEBUG
        std::cerr << " for bextr (SLL_Push).";
#endif
        SLLPush::RegisterEmulation(ins);
        break;
    }
    case XED_ICLASS_BLSR: {
#ifdef TCM_DEBUG
        std::cerr << " for blsr (SizeClassCacheLookup).";
#endif
        SizeClassCacheLookup::RegisterEmulation(ins);
        break;
    }
    case XED_ICLASS_SHLD: {
#ifdef TCM_DEBUG
        std::cerr << " for shld (SizeClassCacheUpdate).";
#endif
        SizeClassCacheUpdate::RegisterEmulation(ins);
        break;
    }
    case XED_ICLASS_SHRX: {
#ifdef TCM_DEBUG
        std::cerr << " for shrx (LLHeadCacheUpdate).";
#endif
        LLHeadCacheUpdate::RegisterEmulation(ins);
        break;
    }
    case XED_ICLASS_SHRD: {
#ifdef TCM_DEBUG
        std::cerr << " for shrd (LLHeadCacheLookup).";
#endif
        LLHeadCacheLookup::RegisterEmulation(ins);
        break;
    }
    case XED_ICLASS_ADC: {
#ifdef TCM_DEBUG
        std::cerr << " for adc (Sampling).";
#endif
        Sampling::RegisterEmulation(ins);
        break;
    }
    default: {
#ifdef TCM_DEBUG
        std::cerr << "...just kidding.";
#endif
        break;
    }
    }

#ifdef TCM_DEBUG
    std::cerr << std::endl;
#endif

    /* Delete placeholder ins on the host side, so we don't get side effects from it. */
    INS_Delete(ins);
}

/* Get realistic replacement actions.
 *
 * Generally, the first magic_insn_action_t object will have do_replace =
 * false, since it is the trigger instruction that we need to simulate as a
 * magic insn. But sometimes this is not true, so the do_replace flag must be
 * set carefully.
 */
static std::vector<magic_insn_action_t> GetRealisticInstructions(const std::vector<INS>& insns,
                                                                 xed_iclass_enum_t iclass) {
    /* For sampling, ignore the rest of the trigger sequence.  But don't ignore
     * the whole taken path (for sampling, IMG instrumentation will take care
     * to only ignore DoSampledAllocation() there).
     */
    switch (iclass) {
    case XED_ICLASS_LZCNT:
        // In the realistic case, SLL_Pop still needs to happen, and the magic
        // instruction SHOULD be replaced.
        return SLLPop::GetBaselineReplacements(insns);
    case XED_ICLASS_BEXTR:
        // Ditto for SLL_Push.
        return SLLPush::GetBaselineReplacements(insns);
    case XED_ICLASS_SHLD:
    case XED_ICLASS_SHRX: {
        // These are magic insns with simulated latency, so don't ignore the
        // trigger (first) instruction!
        auto repl = std::vector<magic_insn_action_t>(insns.size());
        repl[0].do_replace = false;
        return repl;
    }
    case XED_ICLASS_ADC: {
        // We only want to insert the adc when we sample, we'll do it separately.
        auto repl = std::vector<magic_insn_action_t>(insns.size());
        return repl;
    }
    case XED_ICLASS_BLSR:
        return SizeClassCacheLookup::GetRealisticReplacements(insns);
    case XED_ICLASS_SHRD:
        return LLHeadCacheLookup::GetRealisticReplacements(insns);
    default:
        return std::vector<magic_insn_action_t>();
    }
}

static std::vector<magic_insn_action_t> GetBaselineInstructions(const std::vector<INS>& insns,
                                                                xed_iclass_enum_t iclass) {
    switch (iclass) {
    case XED_ICLASS_LZCNT:
        return SLLPop::GetBaselineReplacements(insns);
    case XED_ICLASS_BEXTR:
        return SLLPush::GetBaselineReplacements(insns);
    case XED_ICLASS_SHRD:
        return LLHeadCacheLookup::GetBaselineReplacements(insns);
    case XED_ICLASS_SHRX:
        return LLHeadCacheUpdate::GetBaselineReplacements(insns);
    case XED_ICLASS_BLSR:
        return SizeClassCacheLookup::GetBaselineReplacements(insns);
    case XED_ICLASS_SHLD:
        return SizeClassCacheUpdate::GetBaselineReplacements(insns);
    case XED_ICLASS_ADC:
        return Sampling::GetBaselineReplacements(insns);
    default:
        return std::vector<magic_insn_action_t>();
    }
}

static std::vector<magic_insn_action_t> GetIdealInstructions(const std::vector<INS>& insns,
                                                             xed_iclass_enum_t iclass) {
    switch (iclass) {
    case XED_ICLASS_BEXTR:
    case XED_ICLASS_BLSR:
    case XED_ICLASS_SHRX:
    case XED_ICLASS_SHRD:
    case XED_ICLASS_ADC:
        /* Usually, return an empty list to just ignore all magic. */
        return std::vector<magic_insn_action_t>(insns.size());
    case XED_ICLASS_LZCNT:
        return SLLPop::GetIdealReplacements(insns);
    case XED_ICLASS_SHLD:
        return SizeClassCacheUpdate::GetIdealReplacements(insns);
    default:
        return std::vector<magic_insn_action_t>();
    }
}

/* Take action on the list of trigger instructions (@insns), depending on the
 * @iclass of the main trigger and the configuration (@mode). If ahead-of-time (AOT)
 * instrumentation is required, set instrument_aot to true (default is false). */
static void HandleMagicInsMode(const std::vector<INS>& insns, xed_iclass_enum_t iclass,
                               MagicInsMode mode, bool instrument_aot = false) {
    switch (mode) {
    case IDEAL: {
        auto repl = GetIdealInstructions(insns, iclass);
        /* Ignore all magic instructions (replace them with nothing). */
        for (size_t i = 0; i < insns.size(); i++) {
            if (repl[i].do_replace)
                AddInstructionReplacement(insns[i], repl[i].insns, instrument_aot);
        }

        /* For sampling, ignore everything on the taken branch path. */
        if (iclass == XED_ICLASS_ADC) {
            INS jne = insns.back();
            IgnoreTakenBranchPath(jne);
        }
        break;
    }
    case REALISTIC: {
        /* In general, keep the first trigger instruction and ignore the rest. */
        auto repl = GetRealisticInstructions(insns, iclass);
        ASSERTX(repl.size() == insns.size());
        if (!repl[0].do_replace)
            MarkMagicInstructionHelper(insns[0]);

        /* We start from 0 because some optimizations really DO want to
         * replace the magic instruction. */
        for (size_t i = 0; i < insns.size(); i++) {
            if (repl[i].do_replace)
                AddInstructionReplacement(insns[i], repl[i].insns, instrument_aot);
        }
        break;
    }
    case BASELINE: {
        /* For insns marked as replaced, replace them with the baseline sequence. */
        auto repl = GetBaselineInstructions(insns, iclass);
        ASSERTX(repl.size() == insns.size());
        for (size_t i = 0; i < insns.size(); i++) {
            if (repl[i].do_replace)
                AddInstructionReplacement(insns[i], repl[i].insns, instrument_aot);
        }

        /* For sampling, add extra insns on the branch fallthrough path. */
        if (iclass == XED_ICLASS_ADC) {
            INS jne = insns.back();
            AddFallthroughInstructions(jne, repl.back().insns);
        }
        break;
    }
    default:
        break;
    }
}

void InstrumentTCMHooks(TRACE trace, VOID* v) {
    if (!KnobTCMHooks.Value())
        return;

    freelist_mode = StringToMagicInsMode(KnobFreelistMode.Value());
    size_class_mode = StringToMagicInsMode(KnobSizeClassMode.Value());
    sampling_mode = StringToMagicInsMode(KnobSamplingMode.Value());

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            xed_iclass_enum_t iclass = XED_INS_ICLASS(ins);
            if (iclass != XED_ICLASS_LZCNT &&
                iclass != XED_ICLASS_BEXTR &&
                iclass != XED_ICLASS_BLSR &&
                iclass != XED_ICLASS_SHRX &&
                iclass != XED_ICLASS_ADC &&
                iclass != XED_ICLASS_SHLD &&
                iclass != XED_ICLASS_SHRD)
                continue;

            SEC sec = RTN_Sec(INS_Rtn(ins));
            std::string sec_name = SEC_Name(sec);

            /* The magic instructions don't naturally occur in google_malloc,
             * so any instance we see is fake and should be replaced. */
            if (sec_name != "google_malloc")
                continue;

#ifdef TCM_DEBUG
            std::cerr << "Found placeholder @ pc: " << std::hex << INS_Address(ins) << std::dec
                      << std::dec << " " << INS_Disassemble(ins) << std::endl;
#endif
            InsertEmulationCode(ins, iclass);

            /* Only emulation should run while we're fast-forwarding. */
            if (ExecMode != EXECUTION_MODE_SIMULATE)
                continue;

            auto insns = std::vector<INS>{ins};
            switch (iclass) {
            case XED_ICLASS_LZCNT:
                HandleMagicInsMode(insns, iclass, freelist_mode);
                break;
            case XED_ICLASS_BEXTR:
                HandleMagicInsMode(insns, iclass, freelist_mode);
                break;
            case XED_ICLASS_BLSR:
                insns = SizeClassCacheLookup::LocateMagicSequence(ins);
                HandleMagicInsMode(insns, iclass, size_class_mode);
                break;
            case XED_ICLASS_SHLD:
                insns = SizeClassCacheUpdate::LocateMagicSequence(ins);
                HandleMagicInsMode(insns, iclass, size_class_mode);
                break;
            case XED_ICLASS_ADC:
                insns = Sampling::LocateMagicSequence(ins);
                HandleMagicInsMode(insns, iclass, sampling_mode);
                break;
            case XED_ICLASS_SHRX: {
                HandleMagicInsMode(insns, iclass, freelist_mode);
                break;
            }
            case XED_ICLASS_SHRD:
                // Handle this case in InstrumentTCMIMGHooks.
                break;
            default:

                break;
            }
        }
    }
}

/* Some optimizations have behaviors that require ahead-of-time instrumentation. */
void InstrumentTCMIMGHooks(IMG img) {
    if (!KnobTCMHooks.Value())
        return;

    freelist_mode = StringToMagicInsMode(KnobFreelistMode.Value());
    sampling_mode = StringToMagicInsMode(KnobSamplingMode.Value());

    /* In realistic sampling mode, make sure we ignore DoSampledAllocation.
     * We'll use the magic adc instruction to simulate the (constant) cost of a PMU
     * interrupt, and we'll still simulate PickNextSamplingPoint() as it is, because
     * we need to do it in the real case. */
    if (sampling_mode == REALISTIC) {
        const std::string sampled_sym("_ZL19DoSampledAllocationm");
        RTN rtn = RTN_FindByName(img, sampled_sym.c_str());
        if (RTN_Valid(rtn)) {
#ifdef TCM_DEBUG
            ADDRINT rtn_addr = RTN_Address(rtn);
            std::cerr << "IMG found DoSampledAllocation @ pc: " << std::hex << rtn_addr
                      << std::dec << ". Ignoring in real mode." << std::endl;
#endif
            std::list<xed_encoder_instruction_t> do_sample_repl;
            xed_encoder_instruction_t new_adc;
            xed_inst2(&new_adc, dstate, XED_ICLASS_ADC, xed_mem_op_width,
                      xed_reg(largest_reg(XED_REG_EAX)), // doesn't matter, will turn into a NOP
                      xed_reg(largest_reg(XED_REG_EBX)));
            do_sample_repl.push_back(new_adc);

            AddFunctionReplacement(sampled_sym, 0, do_sample_repl);
        }
    }

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        std::string sec_name = SEC_Name(sec);
        if (sec_name != "google_malloc")
            continue;

        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); RTN_Close(rtn), rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
                xed_iclass_enum_t iclass = XED_INS_ICLASS(ins);
                if (iclass != XED_ICLASS_ADC &&
                    iclass != XED_ICLASS_BLSR &&
                    iclass != XED_ICLASS_SHRD)
                    continue;
#ifdef TCM_DEBUG
                std::cerr << "IMG found placeholder @ pc: " << std::hex << INS_Address(ins)
                          << std::dec << " " << INS_Disassemble(ins) << std::endl;
#endif

                switch (iclass) {
                case XED_ICLASS_ADC:
                    /* When the sampling branch is taken, we don't come back to the fallthrough,
                     * but a few (4-5) instructions above the ret. This is on a different bbl,
                     * so we have to get a bit more creative with the instrumentation to stop
                     * ignoring. We'll just add it statically on all exit points.
                     * This way, we overestimate the benefits by 4-5 insns on the taken path,
                     * but it's ~10K insns, so no harm done. */
                    if (sampling_mode == IDEAL)
                        StopIgnoringTakenBranch(rtn);
                    break;
                case XED_ICLASS_BLSR: {
                        auto insns = SizeClassCacheLookup::LocateMagicSequence(ins);
                        auto fallback = SizeClassCacheLookup::GetFallbackPathBounds(insns, rtn);
#ifdef TCM_DEBUG
                        std::cerr << "Size class fallback path: " << std::endl;
                        for (INS ins : fallback) {
                            std::cerr << std::hex << INS_Address(ins) << std::dec << std::endl;
                        }
#endif
                        if (size_class_mode == IDEAL) {
                            IgnoreBetween(fallback);
                        }
                    break;
                }
                case XED_ICLASS_SHRD: {
                    /* The shrd (LL head lookup) instruction should be placed
                     * BEFORE the fallback is called, but then this causes the shrx
                     * and the test and cmove to be on separate bbls. So we have to
                     * scan the entire routine, find those instructions, and give
                     * them to HandleMagicInsMode.
                     *
                     * The sequence is shrd; ... ; test; cmov[cc].  Either
                     * variant of cmov is possible, as is the ordering of test
                     * and cmov.
                     */
                    auto insns = LLHeadCacheLookup::LocateMagicSequence(ins);
                    HandleMagicInsMode(insns, iclass, freelist_mode, true);
                    break;
                }
                default:
                    break;
                };
            }
        }
    }
}
