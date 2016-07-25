#include <cctype>
#include <iomanip>
#include <iostream>
#include <string>

#include "xiosim/regs.h"
#include "xiosim/decode.h"

#include "BufferManagerProducer.h"
#include "feeder.h"
#include "replace_function.h"
#include "xed_utils.h"

#include "tcm_hooks.h"

enum MagicInsMode {
  IDEAL,
  REALISTIC,
  BASELINE
};


KNOB<BOOL> KnobTCMHooks(KNOB_MODE_WRITEONCE, "pintool", "tcm_hooks", "true",
                        "Emulate tcmalloc replacements.");

KNOB<std::string> KnobSLLPopMode(
        KNOB_MODE_WRITEONCE, "pintool", "sll_pop_mode", "baseline",
        "Simulate the magic lzcnt instruction with either zero latency (ideal), "
        "a realistic accelerator implementation (realistic), or the baseline implementation "
        "(baseline).");
KNOB<std::string> KnobSLLPushMode(
        KNOB_MODE_WRITEONCE, "pintool", "sll_push_mode", "baseline",
        "Desired mode for simulating the magic bextr instruction.");
KNOB<std::string> KnobClassIndexMode(
        KNOB_MODE_WRITEONCE, "pintool", "class_index_mode", "baseline",
        "Desired mode for simulating the magic blsr instruction.");
KNOB<std::string> KnobSamplingMode(
        KNOB_MODE_WRITEONCE, "pintool", "sampling_mode", "baseline",
        "Desired mode for simulating the magic adc sequence.");

/* Helper for our actions for each trigger instruction. */
struct magic_insn_action_t {
    /* Instructions to simulate in addition to the trigger instruction. */
    std::list<xed_encoder_instruction_t> insns;
    /* When true, don't simulate the trigger instruction. */
    bool do_replace;

    magic_insn_action_t()
        : insns()
        , do_replace(true) {}

    magic_insn_action_t(std::list<xed_encoder_instruction_t> insns, bool do_replace)
        : insns(insns)
        , do_replace(do_replace) {}
};

static void InsertEmulationCode(INS ins, xed_iclass_enum_t iclass);
static std::vector<magic_insn_action_t> GetBaselineInstructions(const std::vector<INS>& insns,
                                                                xed_iclass_enum_t iclass);
static std::vector<INS> GetSamplingInstructions(INS adc);

/* Helper to translate from pin registers to xed registers. Why isn't
 * this exposed through the pin API? */
static xed_reg_enum_t PinRegToXedReg(LEVEL_BASE::REG pin_reg) {
    /* Xed and pin registers are ordered diffently.
     * We'll just go through strings for now, instead of building tables.
     * Yeah, it's ugly. */
    ASSERTX(REG_is_gr(pin_reg));
    std::string pin_str = REG_StringShort(pin_reg);
    for (auto& c : pin_str) c = std::toupper(c);
    auto res = str2xed_reg_enum_t(pin_str.c_str());
    ASSERTX(res != XED_REG_INVALID);
    return res;
}

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

/* Convert a knob for magic instruction mode into a MagicInsMode enum. */
static MagicInsMode StringToMagicInsMode(std::string knob_value) {
  if (knob_value == "ideal") {
      return IDEAL;
  } else if (knob_value == "realistic") {
      return REALISTIC;
  } else if (knob_value == "baseline") {
      return BASELINE;
  } else {
      std::cerr << "Invalid value of magic mode knob: " << knob_value << std::endl;
      abort();
  }
}

/* Take action on the list of trigger instructions (@insns), depending on the
 * @iclass of the main trigger and the configuration (@mode). */
static void HandleMagicInsMode(const std::vector<INS>& insns, xed_iclass_enum_t iclass,
                               MagicInsMode mode) {
    switch (mode) {
    case IDEAL: {
        std::list<xed_encoder_instruction_t> empty;
        /* Ignore all magic instructions (replace them with nothing). */
        for (auto ins : insns)
            AddInstructionReplacement(ins, empty);

        /* For sampling, ignore everything on the taken branch path. */
        if (iclass == XED_ICLASS_ADC) {
            INS jne = insns.back();
            IgnoreTakenBranchPath(jne);
        }
        break;
    }
    case REALISTIC: {
        MarkMagicInstructionHelper(insns[0]);

        /* For sampling, ignore the rest of the trigger sequence.
         * But don't ignore the whole taken path (IMG instrumentation
         * will take care to only ignore DoSampledAllocation() there). */
        if (iclass == XED_ICLASS_ADC) {
            std::list<xed_encoder_instruction_t> empty;
            /* Ignore all magic instructions (replace them with nothing). */
            for (size_t i = 1; i < insns.size(); i++)
                AddInstructionReplacement(insns[i], empty);
        }
        break;
    }
    case BASELINE: {
        /* For insns marked as replaced, replace them with the baseline sequence. */
        auto repl = GetBaselineInstructions(insns, iclass);
        ASSERTX(repl.size() == insns.size());
        for (size_t i = 0; i < insns.size(); i++) {
            if (repl[i].do_replace)
                AddInstructionReplacement(insns[i], repl[i].insns);
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

    MagicInsMode pop_mode = StringToMagicInsMode(KnobSLLPopMode.Value());
    MagicInsMode push_mode = StringToMagicInsMode(KnobSLLPushMode.Value());
    MagicInsMode class_index_mode =
            StringToMagicInsMode(KnobClassIndexMode.Value());
    MagicInsMode sampling_mode = StringToMagicInsMode(KnobSamplingMode.Value());

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            xed_iclass_enum_t iclass = static_cast<xed_iclass_enum_t>(INS_Opcode(ins));
            if (iclass != XED_ICLASS_LZCNT &&
                iclass != XED_ICLASS_BEXTR &&
                iclass != XED_ICLASS_BLSR &&
                iclass != XED_ICLASS_ADC)
                continue;

            SEC sec = RTN_Sec(INS_Rtn(ins));
            std::string sec_name = SEC_Name(sec);

            /* The magic instructions don't naturally occur in google_malloc,
             * so any instance we see is fake and should be replaced. */
            if (sec_name != "google_malloc")
                continue;

#ifdef TCM_DEBUG
            std::cerr << "Found placeholder @ pc: " << std::hex << INS_Address(ins) << std::dec << std::endl;
#endif
            InsertEmulationCode(ins, iclass);

            /* Only emulation should run while we're fast-forwarding. */
            if (ExecMode != EXECUTION_MODE_SIMULATE)
                continue;

            auto insns = std::vector<INS>{ins};
            switch (iclass) {
            case XED_ICLASS_LZCNT:
                HandleMagicInsMode(insns, iclass, pop_mode);
                break;
            case XED_ICLASS_BEXTR:
                HandleMagicInsMode(insns, iclass, push_mode);
                break;
            case XED_ICLASS_BLSR:
                HandleMagicInsMode(insns, iclass, class_index_mode);
                break;
            case XED_ICLASS_ADC:
                insns = GetSamplingInstructions(ins);
                HandleMagicInsMode(insns, iclass, sampling_mode);
                break;
            default:
                break;
            }
        }
    }
}

void InstrumentTCMIMGHooks(IMG img) {
    if (!KnobTCMHooks.Value())
        return;

    MagicInsMode sampling_mode = StringToMagicInsMode(KnobSamplingMode.Value());

    /* In realisting sampling mode, make sure we ignore DoSampledAllocation.
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
            std::list<xed_encoder_instruction_t> empty;
            AddFunctionReplacement(sampled_sym, 0, empty);
        }
    }

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        std::string sec_name = SEC_Name(sec);
        if (sec_name != "google_malloc")
            continue;

        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); RTN_Close(rtn), rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {

                xed_iclass_enum_t iclass = static_cast<xed_iclass_enum_t>(INS_Opcode(ins));
                if (iclass != XED_ICLASS_ADC)
                    continue;

#ifdef TCM_DEBUG
                std::cerr << "IMG found placeholder @ pc: " << std::hex << INS_Address(ins)
                          << std::dec << std::endl;
#endif

                /* When the sampling branch is taken, we don't come back to the fallthrough,
                 * but a few (4-5) instructions above the ret. This is on a different bbl,
                 * so we have to get a bit more creative with the instrumentation to stop
                 * ignoring. We'll just add it statically on all exit points.
                 * This way, we overestimate the benefits by 4-5 insns on the taken path,
                 * but it's ~10K insns, so no harm done. */
                if (sampling_mode == IDEAL)
                    StopIgnoringTakenBranch(rtn);
            }
        }
    }
}

/* The placeholder instruction we compile with is just a RR lzcnt dst, src.
 * src holds the address of the LL head, dst will return its contents,
 * and [src] will hold the new head. */
static ADDRINT SLL_Pop_Emulation(ADDRINT head) {
    ADDRINT next;
    ADDRINT result;
    /* Dereference the LL head. */
    PIN_SafeCopy(&result, reinterpret_cast<VOID*>(head), sizeof(ADDRINT));
    /* The head points to next element... */
    PIN_SafeCopy(&next, reinterpret_cast<VOID*>(result), sizeof(ADDRINT));
    /* ...which we store in the head pointer */
    PIN_SafeCopy(reinterpret_cast<VOID*>(head), &next, sizeof(ADDRINT));
    /* Pin will make sure the result of this analysis routine goes to the output reg */
    return result;
}

/* Emulate the ClassIndex() computation from tcmalloc. */
static ADDRINT ClassIndex_Emulation(ADDRINT size) {
    ADDRINT index;
    if (size <= 1024) {
        // Small size classes.
        index = (static_cast<uint32_t>(size) + 7) >> 3;
    } else {
        // Large size classes.
        index = (static_cast<uint32_t>(size) + 127 + (120 << 7)) >> 7;
    }
    return index;
}

/* The placeholder instruction we compile with is a bextr dst, src1, src2.
 * src1 holds the address of the LL head, src2 holds the element we're pushing.
 * [src1] will point to src2, and [src2] will point to old [src1].
 * dst is ignored. */
static void SLL_Push_Emulation(ADDRINT head, ADDRINT element) {
    ADDRINT tmp;
    /* Get that LL head! */
    PIN_SafeCopy(&tmp, reinterpret_cast<VOID*>(head), sizeof(ADDRINT));
    /* Store that head in element! */
    PIN_SafeCopy(reinterpret_cast<VOID*>(element), &tmp, sizeof(ADDRINT));
    /* Look at me, I'm the head now */
    PIN_SafeCopy(reinterpret_cast<VOID*>(head), &element, sizeof(ADDRINT));
}

/*
 * Return value goes to RAX. In host execution, this is followed by a
 * test rax, rax; jne <PickNextSamplingPoint>, so non-zero means we'll
 * sample.
 * */
static bool Sampling_Emulation(ADDRINT bytes_until_sample_addr, ADDRINT size) {
    ADDRINT tmp;
    PIN_SafeCopy(&tmp, reinterpret_cast<VOID*>(bytes_until_sample_addr), sizeof(ADDRINT));
    if (tmp >= size) {
        tmp -= size;
        PIN_SafeCopy(reinterpret_cast<VOID*>(bytes_until_sample_addr), &tmp, sizeof(ADDRINT));
        return 0;
    }
    return 1;
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
        INS_InsertCall(ins,
                       IPOINT_BEFORE,
                       AFUNPTR(SLL_Pop_Emulation),
                       IARG_REG_VALUE,
                       INS_RegR(ins, 0),
                       IARG_RETURN_REGS,
                       INS_RegW(ins, 0),
                       IARG_CALL_ORDER,
                       /* +1, so SLL_Pop_GetMemOperands gets operands *before* any emulation */
                       CALL_ORDER_FIRST + 1,
                       IARG_END);
        break;
    }
    case XED_ICLASS_BEXTR: {
#ifdef TCM_DEBUG
        std::cerr << " for bextr (SLL_Push).";
#endif
        INS_InsertCall(ins,
                       IPOINT_BEFORE,
                       AFUNPTR(SLL_Push_Emulation),
                       IARG_REG_VALUE,
                       INS_RegR(ins, 0),
                       IARG_REG_VALUE,
                       INS_RegR(ins, 1),
                       IARG_CALL_ORDER,
                       /* +1, so SLL_Push_GetMemOperands gets operands *before* any emulation */
                       CALL_ORDER_FIRST + 1,
                       IARG_END);
        break;
    }
    case XED_ICLASS_BLSR: {
#ifdef TCM_DEBUG
        std::cerr << " for blsr (ClassIndex).";
#endif
        INS_InsertCall(ins,
                       IPOINT_BEFORE,
                       AFUNPTR(ClassIndex_Emulation),
                       IARG_REG_VALUE,
                       INS_RegR(ins, 0),
                       IARG_RETURN_REGS,
                       INS_RegW(ins, 0),
                       IARG_CALL_ORDER,
                       CALL_ORDER_FIRST + 1,
                       IARG_END);
        break;
    }
    case XED_ICLASS_ADC: {
#ifdef TCM_DEBUG
        std::cerr << " for adc (Sampling).";
#endif
        INS_InsertCall(ins,
                       IPOINT_BEFORE,
                       AFUNPTR(Sampling_Emulation),
                       IARG_MEMORYOP_EA,
                       0,
                       IARG_REG_VALUE,
                       INS_RegR(ins, 1),
                       IARG_RETURN_REGS,
                       LEVEL_BASE::REG_FullRegName(LEVEL_BASE::REG_EAX),
                       IARG_CALL_ORDER,
                       CALL_ORDER_FIRST + 1,
                       IARG_END);

        /* We also have to delete the lahf on the host, so the following test
         * and jump (that we leave) check the result of the emulation routine. */
        auto insns = GetSamplingInstructions(ins);
        INS lahf = insns[1];
        INS_Delete(lahf);
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

/* Analysis routine to prepare memory addresses for a potential replacement. */
static void SLL_Pop_GetMemOperands(THREADID tid, ADDRINT pc, ADDRINT head) {
    ADDRINT result;
    /* Dereference the LL head. */
    PIN_SafeCopy(&result, reinterpret_cast<VOID*>(head), sizeof(ADDRINT));

    thread_state_t* tstate = get_tls(tid);
    tstate->replacement_mem_ops[pc].clear();
    tstate->replacement_mem_ops.at(pc).push_back({head, sizeof(ADDRINT)});
    tstate->replacement_mem_ops.at(pc).push_back({result, sizeof(ADDRINT)});
    tstate->replacement_mem_ops.at(pc).push_back({head, sizeof(ADDRINT)});
}

/* Helper for the baseline SLL_Pop sequence.
 * Returns the xed-encoded list of instructions that make up SLL_Pop.
 * Sets up an analysis routine to grab the correspoining memory operands. */
static std::vector<magic_insn_action_t> Prepare_SLL_PopSimulation(const std::vector<INS>& insns) {
    INS lzcnt = insns.front();
    REG head_reg = INS_RegR(lzcnt, 0);
    REG res_reg = INS_RegW(lzcnt, 0);

    INS_InsertCall(lzcnt,
                   IPOINT_BEFORE,
                   AFUNPTR(SLL_Pop_GetMemOperands),
                   IARG_THREAD_ID,
                   IARG_INST_PTR,
                   IARG_REG_VALUE,
                   head_reg,
                   IARG_CALL_ORDER,
                   CALL_ORDER_FIRST,
                   IARG_END);

    xed_reg_enum_t head_reg_xed = PinRegToXedReg(head_reg);
    xed_reg_enum_t res_reg_xed = PinRegToXedReg(res_reg);

    std::list<xed_encoder_instruction_t> repl;
    xed_encoder_instruction_t get_head;
    xed_inst2(&get_head, dstate, XED_ICLASS_MOV, xed_mem_op_width,
              xed_reg(res_reg_xed),
              xed_mem_b(head_reg_xed, xed_mem_op_width));
    repl.push_back(get_head);

    xed_encoder_instruction_t get_next;
    xed_inst2(&get_next, dstate, XED_ICLASS_MOV, xed_mem_op_width,
              /* We really want a temp register here, but don't know
               * what's available at instrumentation time. */
              xed_reg(largest_reg(XED_REG_R15D)),
              xed_mem_b(res_reg_xed, xed_mem_op_width));
    repl.push_back(get_next);

    xed_encoder_instruction_t update_head;
    xed_inst2(&update_head, dstate, XED_ICLASS_MOV, xed_mem_op_width,
              xed_mem_b(head_reg_xed, xed_mem_op_width),
              xed_reg(largest_reg(XED_REG_R15D)));
    repl.push_back(update_head);

    std::vector<magic_insn_action_t> result;
    result.emplace_back(repl, true);
    return result;
}

/* Analysis routine to prepare memory addresses for a potential replacement. */
static void SLL_Push_GetMemOperands(THREADID tid, ADDRINT pc, ADDRINT head, ADDRINT element) {
    thread_state_t* tstate = get_tls(tid);
    tstate->replacement_mem_ops[pc].clear();
    tstate->replacement_mem_ops.at(pc).push_back({head, sizeof(ADDRINT)});
    tstate->replacement_mem_ops.at(pc).push_back({element, sizeof(ADDRINT)});
    tstate->replacement_mem_ops.at(pc).push_back({head, sizeof(ADDRINT)});
}

/* Helper for the baseline SLL_Push sequence.
 * Returns the xed-encoded list of instructions that make up SLL_Push.
 * Sets up an analysis routine to grab the correspoining memory operands. */
std::vector<magic_insn_action_t> Prepare_SLL_PushSimulation(const std::vector<INS>& insns) {
    INS bextr = insns.front();
    REG head_reg = INS_RegR(bextr, 0);
    REG element_reg = INS_RegR(bextr, 1);

    INS_InsertCall(bextr,
                   IPOINT_BEFORE,
                   AFUNPTR(SLL_Push_GetMemOperands),
                   IARG_THREAD_ID,
                   IARG_INST_PTR,
                   IARG_REG_VALUE,
                   head_reg,
                   IARG_REG_VALUE,
                   element_reg,
                   IARG_CALL_ORDER,
                   CALL_ORDER_FIRST,
                   IARG_END);

    xed_reg_enum_t head_reg_xed = PinRegToXedReg(head_reg);
    xed_reg_enum_t element_reg_xed = PinRegToXedReg(element_reg);

    std::list<xed_encoder_instruction_t> repl;
    xed_encoder_instruction_t get_head;
    xed_inst2(&get_head, dstate, XED_ICLASS_MOV, xed_mem_op_width,
              /* We really want a temp register here, but don't know
               * what's available at instrumentation time. */
              xed_reg(largest_reg(XED_REG_R15D)),
              xed_mem_b(head_reg_xed, xed_mem_op_width));
    repl.push_back(get_head);

    xed_encoder_instruction_t store_head;
    xed_inst2(&store_head, dstate, XED_ICLASS_MOV, xed_mem_op_width,
              xed_mem_b(element_reg_xed, xed_mem_op_width),
              xed_reg(largest_reg(XED_REG_R15D)));
    repl.push_back(store_head);

    xed_encoder_instruction_t update_head;
    xed_inst2(&update_head, dstate, XED_ICLASS_MOV, xed_mem_op_width,
              xed_mem_b(head_reg_xed, xed_mem_op_width),
              xed_reg(element_reg_xed));
    repl.push_back(update_head);

    std::vector<magic_insn_action_t> result;
    result.emplace_back(repl, true);
    return result;
}

std::vector<magic_insn_action_t> Prepare_ClassIndex_Simulation(const std::vector<INS>& insns) {
    INS blsr = insns.front();
    REG size_reg = INS_RegR(blsr, 0);
    REG index_reg = INS_RegW(blsr, 0);

    xed_reg_enum_t size_reg_xed = PinRegToXedReg(size_reg);
    xed_reg_enum_t index_reg_xed = PinRegToXedReg(index_reg);

    std::list<xed_encoder_instruction_t> repl;

    /* Compute small size class index. */
    xed_encoder_instruction_t lea_7;
    xed_inst2(&lea_7, dstate, XED_ICLASS_LEA, xed_mem_op_width,
              xed_reg(index_reg_xed),
              xed_mem_bd(size_reg_xed, xed_disp(7, 32), xed_mem_op_width));

    xed_encoder_instruction_t shr_3;
    xed_inst2(&shr_3, dstate, XED_ICLASS_SHR, xed_mem_op_width,
              xed_reg(index_reg_xed),
              xed_imm0(3, 8));

    /* Compute large size class index. */
    xed_encoder_instruction_t lea_3c7f;
    xed_inst2(&lea_3c7f, dstate, XED_ICLASS_LEA, xed_mem_op_width,
              xed_reg(index_reg_xed),
              xed_mem_bd(size_reg_xed, xed_disp(0x3c7f, 32), xed_mem_op_width));

    xed_encoder_instruction_t shr_7;
    xed_inst2(&shr_7, dstate, XED_ICLASS_SHR, xed_mem_op_width,
              xed_reg(index_reg_xed),
              xed_imm0(7, 8));

    /* Check if small size class or large size class. */
    xed_encoder_instruction_t cmp;
    xed_inst2(&cmp, dstate, XED_ICLASS_CMP, xed_mem_op_width,
              xed_reg(size_reg_xed),
              xed_imm0(0x400, 32));

#ifdef USE_FDO_BUILD
    /* If we are running FDO builds of tcmalloc, the optimized code path for
     * class index computation uses compares and jumps. */
    uint8_t buf[xiosim::x86::MAX_ILEN];
    size_t lea0_len = Encode(lea_7, &buf[0]);
    size_t shr0_len = Encode(shr_3, &buf[0]);
    size_t lea1_len = Encode(lea_3c7f, &buf[0]);
    size_t shr1_len = Encode(shr_7, &buf[0]);

    /* Jump over large size class index instructions. */
    xed_encoder_instruction_t jmp_over_large_sz;
    xed_inst1(&jmp_over_large_sz, dstate, XED_ICLASS_JMP, 0,
              xed_relbr(lea1_len + shr1_len, 8));
    size_t jmp_over_len = Encode(jmp_over_large_sz, &buf[0]);

    /* Jump based on size class check. */
    xed_encoder_instruction_t jmp_to_large_sz;
    xed_inst1(&jmp_to_large_sz, dstate, XED_ICLASS_JNBE, 0,
              xed_relbr(lea0_len + shr0_len + jmp_over_len, 8));

    /* Add instructions in order. */
    repl.push_back(cmp);
    repl.push_back(jmp_to_large_sz);
    repl.push_back(lea_7);
    repl.push_back(shr_3);
    repl.push_back(jmp_over_large_sz);
    repl.push_back(lea_3c7f);
    repl.push_back(shr_7);
#else
    /* Without FDO, the optimized code path computes class indices for both
     * small and large size classes and uses a compare and conditional move to
     * select the right one. */

    /* Move small size class repl to another register. */
    xed_encoder_instruction_t mov_small;
    xed_inst2(&mov_small, dstate, XED_ICLASS_MOV, xed_mem_op_width,
              xed_reg(largest_reg(XED_REG_R15D)),
              xed_reg(index_reg_xed));

    /* Conditional move based on size class type. */
    xed_encoder_instruction_t cmov;
    xed_inst2(&cmov, dstate, XED_ICLASS_CMOVBE, xed_mem_op_width,
              xed_reg(index_reg_xed),
              xed_reg(largest_reg(XED_REG_R15D)));

    repl.push_back(lea_7);
    repl.push_back(shr_3);
    repl.push_back(mov_small);
    repl.push_back(lea_3c7f);
    repl.push_back(shr_7);
    repl.push_back(cmp);
    repl.push_back(cmov);
#endif

    std::vector<magic_insn_action_t> result;
    result.emplace_back(repl, true);
    return result;
}

/* For sampling, the magic sequence we insert isn't a single instruction.
 * Grab the adc; lahf; test; jne; sequence that we'll be modifying.
 * They are all one the same bbl, so we can do this without gymnastics. */
static std::vector<INS> GetSamplingInstructions(INS adc) {
    std::vector<INS> result{adc};

    INS lahf = INS_Next(adc);
    ASSERTX(INS_Valid(lahf));
    xed_iclass_enum_t lahf_iclass = static_cast<xed_iclass_enum_t>(INS_Opcode(lahf));
    ASSERTX(lahf_iclass == XED_ICLASS_LAHF);
    result.push_back(lahf);

    /* ... and let's make sure the lahf is always followed by the test and jump. */
    INS test = INS_Next(lahf);
    ASSERTX(INS_Valid(test));
    xed_iclass_enum_t test_iclass = static_cast<xed_iclass_enum_t>(INS_Opcode(test));
    ASSERTX(test_iclass == XED_ICLASS_TEST);
    result.push_back(test);

    /* In some cases (do_malloc_pages()), the optimizer sneaks in an independent
     * instruction in our sequence. Look for the jump until we hit the end of the bbl. */
    INS jne = INS_Next(test);
    while (INS_Valid(jne)) {
        xed_iclass_enum_t jne_iclass = static_cast<xed_iclass_enum_t>(INS_Opcode(jne));
        if (jne_iclass == XED_ICLASS_JNZ) {
            result.push_back(jne);
            ASSERTX(INS_HasFallThrough(jne));
            break;
        }
        jne = INS_Next(jne);
    }
    /* We've reached the end of the bbl without finding the jne. Uh-oh. */
    ASSERTX(INS_Valid(jne));

    return result;
}

/* Analysis routine to prepare memory addresses for a potential replacement. */
static void Sampling_GetMemOperands(THREADID tid, ADDRINT pc, ADDRINT jne_pc, ADDRINT addr) {
    thread_state_t* tstate = get_tls(tid);
    tstate->replacement_mem_ops[pc].clear();
    tstate->replacement_mem_ops.at(pc).push_back({addr, sizeof(ADDRINT)});

    tstate->replacement_mem_ops[jne_pc].clear();
    tstate->replacement_mem_ops.at(jne_pc).push_back({addr, sizeof(ADDRINT)});
}

/* Helper for the sampling baseline sequence. */
std::vector<magic_insn_action_t> Prepare_Sampling_Simulation(const std::vector<INS>& insns) {
    INS adc = insns.front();
    REG addr_reg = INS_RegR(adc, 0);
    REG size_reg = INS_RegR(adc, 1);

    INS jne = insns.back();
    INS_InsertCall(adc,
                   IPOINT_BEFORE,
                   AFUNPTR(Sampling_GetMemOperands),
                   IARG_THREAD_ID,
                   IARG_INST_PTR,
                   IARG_ADDRINT,
                   INS_Address(jne),
                   IARG_MEMORYOP_EA,
                   0,
                   IARG_CALL_ORDER,
                   CALL_ORDER_FIRST,
                   IARG_END);

    xed_reg_enum_t addr_reg_xed = PinRegToXedReg(addr_reg);
    xed_reg_enum_t size_reg_xed = PinRegToXedReg(size_reg);

    std::vector<magic_insn_action_t> result;
    std::list<xed_encoder_instruction_t> adc_repl;
    xed_encoder_instruction_t load_bus;
    xed_inst2(&load_bus, dstate, XED_ICLASS_MOV, xed_mem_op_width,
              xed_reg(largest_reg(XED_REG_EAX)),
              xed_mem_b(addr_reg_xed, xed_mem_op_width));
    adc_repl.push_back(load_bus);

    xed_encoder_instruction_t cmp_bus_size;
    xed_inst2(&cmp_bus_size, dstate, XED_ICLASS_CMP, xed_mem_op_width,
              xed_reg(largest_reg(XED_REG_EAX)),
              xed_reg(size_reg_xed));
    adc_repl.push_back(cmp_bus_size);
    /* Replace the adc with load; cmp. */
    result.emplace_back(adc_repl, true);

    /* Empty list to just ignore lahf. */
    result.emplace_back();
    /* Ditto for test. */
    result.emplace_back();

    list<xed_encoder_instruction_t> jne_ft_insns;
    xed_encoder_instruction_t sub_bus_size;
    xed_inst2(&sub_bus_size, dstate, XED_ICLASS_SUB, xed_mem_op_width,
              xed_reg(largest_reg(XED_REG_EAX)),
              xed_reg(size_reg_xed));
    jne_ft_insns.push_back(sub_bus_size);

    xed_encoder_instruction_t store_bus;
    xed_inst2(&store_bus, dstate, XED_ICLASS_MOV, xed_mem_op_width,
              xed_mem_b(addr_reg_xed, xed_mem_op_width),
              xed_reg(largest_reg(XED_REG_EAX)));
    jne_ft_insns.push_back(store_bus);
    /* Don't ignore the branch. We'll just add the extra ft sub; store. */
    result.emplace_back(jne_ft_insns, false);

    return result;
}

static std::vector<magic_insn_action_t> GetBaselineInstructions(const std::vector<INS>& insns,
                                                               xed_iclass_enum_t iclass) {
    switch (iclass) {
    case XED_ICLASS_LZCNT:
        return Prepare_SLL_PopSimulation(insns);
    case XED_ICLASS_BEXTR:
        return Prepare_SLL_PushSimulation(insns);
    case XED_ICLASS_BLSR:
        return Prepare_ClassIndex_Simulation(insns);
    case XED_ICLASS_ADC:
        return Prepare_Sampling_Simulation(insns);
    default:
        return std::vector<magic_insn_action_t>();
    }
}
