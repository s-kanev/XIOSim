#include <list>
#include <vector>

#include "xiosim/regs.h"
#include "xiosim/pintool/BufferManagerProducer.h"
#include "xiosim/pintool/feeder.h"
#include "xiosim/pintool/xed_utils.h"

#include "tcm_opts.h"
#include "tcm_utils.h"

MagicInsMode size_class_mode;

namespace SizeClassCacheLookup {

/* Rather than emulating the size class computation itself, we instruct the
 * host whether or not to take the branch that executes the fallback code. This
 * depends on the value of KnobSizeClassMode and ExecMode.  If we are not
 * simulating, always take the branch.
 *
 * Otherwise, if KnobSizeClassMode is:
 *   - baseline: always take the branch.
 *   - ideal: always take the branch.
 *   - realistic: Consult the SizeClassCache and branch if we do not hit in the cache.
 *
 * If the lookup hits in the cache, return the size class and write the
 * allocated size into the size register. If the lookup fails, return 0 and do
 * not modify size_reg.
 */
static ADDRINT Emulation(THREADID tid, ADDRINT size, PIN_REGISTER* size_reg) {
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return 0;
    switch (size_class_mode) {
    case BASELINE:
    case IDEAL:
        return 0;
    case REALISTIC: {
        thread_state_t* tstate = get_tls(tid);
        cache_entry_t result;
        bool found = tstate->size_class_cache.size_lookup(size, result);
        if (found) {
            size_reg->qword[0] = static_cast<UINT64>(result.get_size());
            ASSERTX(result.get_size_class() > 0);
        }
#ifdef EMULATION_DEBUG
        std::cerr << "Emulating SizeClassCacheLookup: size=" << size << ", found=" << found
                  << std::endl;
#endif
        return result.get_size_class();
    }
    }
    return 0;
}

/* Pass cache lookup input operands to the timing simulator. */
static VOID GetRegOperands(THREADID tid, ADDRINT size) {
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;
    switch (size_class_mode) {
    case BASELINE:
    case IDEAL:
        return;
    case REALISTIC: {
        thread_state_t* tstate = get_tls(tid);
        // Abusing mem_buffer to store the requested size class (input operand
        // of this magic instruction).
        auto handshake = xiosim::buffer_management::GetBuffer(tstate->tid);
        handshake->mem_buffer.push_back(std::make_pair(static_cast<md_addr_t>(size), 0));
    }
    }
}

void RegisterEmulation(INS ins) {
    // We will delete the blsr on the host but leave the test instruction
    // so the host takes the right path.
    INS_InsertCall(ins,
                   IPOINT_BEFORE,
                   AFUNPTR(GetRegOperands),
                   IARG_THREAD_ID,
                   IARG_REG_VALUE,
                   INS_RegR(ins, 0),
                   IARG_CALL_ORDER,
                   CALL_ORDER_FIRST,
                   IARG_END);
    INS_InsertCall(ins,
                   IPOINT_BEFORE,
                   AFUNPTR(Emulation),
                   IARG_THREAD_ID,
                   IARG_REG_VALUE,
                   INS_RegR(ins, 0),
                   IARG_REG_REFERENCE,
                   INS_RegR(ins, 0),
                   IARG_RETURN_REGS,
                   INS_RegW(ins, 0),
                   IARG_CALL_ORDER,
                   CALL_ORDER_FIRST + 1,
                   IARG_END);
}

/* For simulating the baseline size class, we will just skip the three magic
 * instructions. The host will force the branch to be taken and go to the
 * software fallback.
 */
repl_vec_t GetBaselineReplacements(const insn_vec_t& insns) {
    std::vector<magic_insn_action_t> result;
    // We may or may not find the mov in between blsr and test.
    ASSERTX(insns.size() == 3 || insns.size() == 4);
    for (unsigned i = 0; i < insns.size(); i++)
        result.emplace_back();

    return result;
}

/* The blsr instruction will set a flag based on the result of the lookup, so
 * we can ignore the test instruction that would do the same thing.
 */
repl_vec_t GetRealisticReplacements(const insn_vec_t& insns) {
    ASSERTX(insns.size() == 3 || insns.size() == 4);
    std::list<xed_encoder_instruction_t> empty;
    std::vector<magic_insn_action_t> result;
    result.emplace_back(empty, false);  // Don't ignore the blsr.
    result.emplace_back();              // Ignore the test.
    if (insns.size() == 4)
        result.emplace_back();          // Ignore the mov.
    result.emplace_back(empty, false);  // Don't ignore the jump.
    return result;
}

/* For size class, the magic instruction sequence is blsr; mov; test; j(n)e.  Grab
 * this instruction sequence so we can handle them appropriately.
 */
insn_vec_t LocateMagicSequence(const INS& ins) {
    const INS& blsr = ins;
    std::vector<INS> result{ blsr };

    // There might be something innocent between the blsr and test
    INS test = GetNextInsOfClass(blsr, XED_ICLASS_TEST);
    ASSERTX(INS_Valid(test));
    result.push_back(test);

    // Try to find the reg-reg mov that reads the input of the blsr. Sometimes
    // it doesn't exist.
    LEVEL_BASE::REG blsr_src = INS_RegR(blsr, 0);
    INS next = INS_Next(blsr);
    while (INS_Valid(next)) {
        next = GetNextInsOfClass(next, XED_ICLASS_MOV);
        if (INS_RegRContain(next, blsr_src)) {
            result.push_back(next);
            break;
        }
        next = INS_Next(next);
    }

    INS jene = GetNextZFlagBranch(test);
    ASSERTX(INS_Valid(jene));
    result.push_back(jene);

    return result;
}

/* Helper to find insn with @pc in @rtn. */
static INS RTN_FindInsByAddress(RTN rtn, ADDRINT pc) {
    ASSERTX(pc < RTN_Address(rtn) + RTN_Size(rtn));
    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
        if (INS_Address(ins) == pc)
            return ins;
    }
    return INS();
}

insn_vec_t GetFallbackPathBounds(const insn_vec_t& insns, RTN rtn) {
    INS jene = insns.back();
    bool is_je = (XED_INS_ICLASS(jene) == XED_ICLASS_JZ);

    insn_vec_t result;
    if (is_je) {
        /* JE -- falback path is the taken direction, and ends with a jmp to the ft. */
        ADDRINT je_target_pc = INS_DirectBranchOrCallTargetAddress(jene);
        INS je_target = RTN_FindInsByAddress(rtn, je_target_pc);
        result.push_back(je_target);

        /* Except when there's no jmp, and we have e.g. another exit path. */
        /* We'll just play along with the shld again. */
        INS shld = GetNextInsOfClass(je_target, XED_ICLASS_SHLD);
        ASSERTX(INS_Valid(shld));
        INS shld_next = INS_Next(shld);
        ASSERTX(INS_Valid(shld_next));

        if (XED_INS_ICLASS(shld_next) != XED_ICLASS_JMP) {
            result.push_back(shld_next);
        } else {
            ADDRINT je_ft_pc = INS_NextAddress(jene);
            INS je_ft = RTN_FindInsByAddress(rtn, je_ft_pc);
            result.push_back(je_ft);
        }
    } else {
        /* JNE -- fallback path is on the jne fallthrough, and (typically) ends at the jne target. */
        ADDRINT jne_ft_pc = INS_NextAddress(jene);
        INS jne_ft = RTN_FindInsByAddress(rtn, jne_ft_pc);
        result.push_back(jne_ft);

        /* Except in some crazy corner cases like MarkThreadBusy() */
        /* Then, we'll just stop at the ins after the shld, and print a warning. */
        INS shld = GetNextInsOfClass(jne_ft, XED_ICLASS_SHLD);
        if (!INS_Valid(shld)) {
            /* Mkay, in some even more fun cases the shld is a direct jump away.
             * So, in desperation, just go for any shld in this routine. */
            shld = GetNextInsOfClass(RTN_InsHead(rtn), XED_ICLASS_SHLD);
            ASSERTX(INS_Valid(shld));
        }
        INS shld_next = INS_Next(shld);
        ASSERTX(INS_Valid(shld_next));
        result.push_back(shld_next);

        ADDRINT jne_target_pc = INS_DirectBranchOrCallTargetAddress(jene);
        if (INS_Address(shld_next) != jne_target_pc)
            std::cerr << "Size class fallback: corner case fallback found at "
                      << std::hex << INS_Address(shld_next) << std::dec << std::endl;
    }
    return result;
}

}  // namespace SizeClassCacheLookup

namespace SizeClassCacheUpdate {

/* Update the size class cache on the producer. */
static VOID Emulation(THREADID tid, ADDRINT orig_size, ADDRINT size, ADDRINT cl) {
    // If we're not simulating, none of these optimizations matter.
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;
    switch (size_class_mode) {
    case BASELINE:
    case IDEAL:
        return;
    case REALISTIC: {
        thread_state_t* tstate = get_tls(tid);
        bool success = tstate->size_class_cache.size_update(orig_size, size, cl);
#ifdef EMULATION_DEBUG
        std::cerr << "Emulating SizeClassCacheUpdate: orig_size=" << orig_size << ", size=" << size
                  << ", class=" << cl << ", success=" << success << std::endl;
#else
        (void)success;
#endif

        return;
    }
    default:
        return;
    }
}

/* Pass cache update input operands to the timing simulator. */
static VOID GetRegOperands(THREADID tid, ADDRINT orig_size, ADDRINT size, ADDRINT cl) {
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;
    switch (size_class_mode) {
    case BASELINE:
    case IDEAL:
        return;
    case REALISTIC: {
        thread_state_t* tstate = get_tls(tid);
        auto handshake = xiosim::buffer_management::GetBuffer(tstate->tid);
        // Ugly, but we need three parameters, so we need two pairs.
        handshake->mem_buffer.push_back(std::make_pair(static_cast<md_addr_t>(orig_size), 0));
        handshake->mem_buffer.push_back(
                std::make_pair(static_cast<md_addr_t>(size), static_cast<uint8_t>(cl)));
    }
    }
}

void RegisterEmulation(INS ins) {
    INS_InsertCall(ins,
                   IPOINT_BEFORE,
                   AFUNPTR(GetRegOperands),
                   IARG_THREAD_ID,
                   IARG_REG_VALUE,
                   INS_RegW(ins, 0),
                   IARG_REG_VALUE,
                   INS_RegR(ins, 1),
                   IARG_REG_VALUE,
                   INS_RegR(ins, 2),
                   IARG_CALL_ORDER,
                   CALL_ORDER_FIRST,
                   IARG_END);
    INS_InsertCall(ins,
                   IPOINT_BEFORE,
                   AFUNPTR(Emulation),
                   IARG_THREAD_ID,
                   IARG_REG_VALUE,
                   INS_RegW(ins, 0),
                   IARG_REG_VALUE,
                   INS_RegR(ins, 1),
                   IARG_REG_VALUE,
                   INS_RegR(ins, 2),
                   IARG_CALL_ORDER,
                   CALL_ORDER_FIRST + 1,
                   IARG_END);
}

static void CheckSequence(const insn_vec_t& insns) {
    ASSERTX(insns.size() >= 1 || insns.size() <= 3);
    if (insns.size() == 1) {
        ASSERTX(XED_INS_ICLASS(insns[0]) == XED_ICLASS_SHLD);
    }
    else if (insns.size() == 2) {
        ASSERTX(XED_INS_ICLASS(insns[0]) == XED_ICLASS_SHLD);
        ASSERTX(XED_INS_ICLASS(insns[1]) == XED_ICLASS_JMP || XED_INS_ICLASS(insns[1]) == XED_ICLASS_MOV);
    } else if (insns.size() == 3) {
        ASSERTX(XED_INS_ICLASS(insns[0]) == XED_ICLASS_SHLD);
        ASSERTX(XED_INS_ICLASS(insns[1]) == XED_ICLASS_MOV);
        ASSERTX(XED_INS_ICLASS(insns[2]) == XED_ICLASS_JMP);
    }
}

/* For the size class cache update magic instruction sequence, we want both the
 * shld and the jump ignored for the baseline.
 */
repl_vec_t GetBaselineReplacements(const insn_vec_t& insns) {
    CheckSequence(insns);

    std::vector<magic_insn_action_t> result;
    for (unsigned i = 0; i < insns.size(); i++) {
        result.emplace_back();
    }
    return result;
}

/* In the ideal case, lookup instrumentation already takes care of ignoring
 * the whole branch, so we don't do anything, lest we stop ignoring by accident.
 */
repl_vec_t GetIdealReplacements(const insn_vec_t& insns) {
    CheckSequence(insns);

    std::list<xed_encoder_instruction_t> empty;
    std::vector<magic_insn_action_t> result;
    for (unsigned i = 0; i < insns.size(); i++) {
        result.emplace_back(empty, false);
    }
    return result;
}

/* For updating the size class cache, get the mov rcx, r64; shld; jmp magic
 * instruction sequence. The mov sets up the cl operand for shld.
 *
 * The returned vector contains shld as the first element, so that later steps
 * can assume the first element of any magic sequence is the trigger
 * instruction. The order isn't that important anyways since we don't depend on
 * it for inserting analysis routines.
 */
insn_vec_t LocateMagicSequence(const INS& ins) {
    const INS& shld = ins;
    std::vector<INS> result{ shld };
    // First go backwards to find the mov.
    INS prev = INS_Prev(shld);
    while (INS_Valid(prev)) {
        prev = GetPrevInsOfClass(prev, XED_ICLASS_MOV);
        if (INS_RegWContain(prev, REG_FullRegName(LEVEL_BASE::REG_ECX))) {
            result.push_back(prev);
            break;
        }
        prev = INS_Prev(prev);
    }

    /* jmp might or might not be there */
    INS jmp = INS_Next(shld);
    if (INS_Valid(jmp) && XED_INS_ICLASS(jmp) == XED_ICLASS_JMP)
        result.push_back(jmp);

    return result;
}

}  // namespace SizeClassCacheUpdate

// Deprecated.
namespace ClassIndex {

repl_vec_t GetBaselineReplacements(const insn_vec_t& insns) {
    INS blsr = insns.front();
    REG size_reg = INS_RegR(blsr, 0);
    REG index_reg = INS_RegW(blsr, 0);

    xed_reg_enum_t size_reg_xed = PinRegToXedReg(size_reg);
    xed_reg_enum_t index_reg_xed = PinRegToXedReg(index_reg);

    std::list<xed_encoder_instruction_t> repl;

    /* Compute small size class index. */
    xed_encoder_instruction_t lea_7;
    xed_inst2(&lea_7, dstate, XED_ICLASS_LEA, xed_mem_op_width, xed_reg(index_reg_xed),
              xed_mem_bd(size_reg_xed, xed_disp(7, 32), xed_mem_op_width));

    xed_encoder_instruction_t shr_3;
    xed_inst2(&shr_3, dstate, XED_ICLASS_SHR, xed_mem_op_width, xed_reg(index_reg_xed),
              xed_imm0(3, 8));

    /* Compute large size class index. */
    xed_encoder_instruction_t lea_3c7f;
    xed_inst2(&lea_3c7f, dstate, XED_ICLASS_LEA, xed_mem_op_width, xed_reg(index_reg_xed),
              xed_mem_bd(size_reg_xed, xed_disp(0x3c7f, 32), xed_mem_op_width));

    xed_encoder_instruction_t shr_7;
    xed_inst2(&shr_7, dstate, XED_ICLASS_SHR, xed_mem_op_width, xed_reg(index_reg_xed),
              xed_imm0(7, 8));

    /* Check if small size class or large size class. */
    xed_encoder_instruction_t cmp;
    xed_inst2(&cmp, dstate, XED_ICLASS_CMP, xed_mem_op_width, xed_reg(size_reg_xed),
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
    xed_inst1(&jmp_over_large_sz, dstate, XED_ICLASS_JMP, 0, xed_relbr(lea1_len + shr1_len, 8));
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
              xed_reg(largest_reg(XED_REG_R15D)), xed_reg(index_reg_xed));

    /* Conditional move based on size class type. */
    xed_encoder_instruction_t cmov;
    xed_inst2(&cmov, dstate, XED_ICLASS_CMOVBE, xed_mem_op_width, xed_reg(index_reg_xed),
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

}  // namespace ClassIndex
