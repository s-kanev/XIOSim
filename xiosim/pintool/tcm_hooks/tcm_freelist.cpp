#include <list>
#include <vector>

#include "xiosim/regs.h"
#include "xiosim/pintool/BufferManagerProducer.h"
#include "xiosim/pintool/feeder.h"
#include "xiosim/pintool/xed_utils.h"

#include "tcm_utils.h"

MagicInsMode freelist_mode;

// Free list length is stored right after to the head pointer.
static int GetFreeListLength(ADDRINT head) {
    ADDRINT length_ptr = head + sizeof(ADDRINT);
    ADDRINT length;
    PIN_SafeCopy(&length, reinterpret_cast<VOID*>(length_ptr), sizeof(ADDRINT));
    length &= 0xffffffff;  // Length is stored as a 32-bit value.
    return static_cast<int>(length);
}

namespace SLLPop {

/* Analysis routine to get the next head to prefetch. */
static void GetIdealMemOperands(THREADID tid, ADDRINT pc, ADDRINT head) {
    ADDRINT result;
    /* Dereference the LL head. */
    PIN_SafeCopy(&result, reinterpret_cast<VOID*>(head), sizeof(ADDRINT));

    thread_state_t* tstate = get_tls(tid);
    tstate->replacement_mem_ops[pc].clear();
    tstate->replacement_mem_ops.at(pc).push_back({result, sizeof(ADDRINT)});
}

/* Analysis routine to prepare memory addresses for a potential replacement. */
static void GetBaselineMemOperands(THREADID tid, ADDRINT pc, ADDRINT head) {
    ADDRINT result;
    /* Dereference the LL head. */
    PIN_SafeCopy(&result, reinterpret_cast<VOID*>(head), sizeof(ADDRINT));

    thread_state_t* tstate = get_tls(tid);
    tstate->replacement_mem_ops[pc].clear();
    tstate->replacement_mem_ops.at(pc).push_back({head, sizeof(ADDRINT)});
    tstate->replacement_mem_ops.at(pc).push_back({result, sizeof(ADDRINT)});
    tstate->replacement_mem_ops.at(pc).push_back({head, sizeof(ADDRINT)});
}

/* The placeholder instruction we compile with is just a RR lzcnt dst, src.
 * src holds the address of the LL head, dst will return its contents,
 * and [src] will hold the new head. */
static ADDRINT Emulation(ADDRINT head) {
#ifdef EMULATION_DEBUG
    std::cerr << "Emulating SLLPop: head=0x" << std::hex << head;
#endif
    ADDRINT next;
    ADDRINT result;
    /* Dereference the LL head. */
    PIN_SafeCopy(&result, reinterpret_cast<VOID*>(head), sizeof(ADDRINT));
    /* The head points to next element... */
    PIN_SafeCopy(&next, reinterpret_cast<VOID*>(result), sizeof(ADDRINT));
    /* ...which we store in the head pointer */
    PIN_SafeCopy(reinterpret_cast<VOID*>(head), &next, sizeof(ADDRINT));
    /* Pin will make sure the result of this analysis routine goes to the output reg */
#ifdef EMULATION_DEBUG
    std::cerr << ", result=0x" << result << ", next=0x" << next << std::dec << std::endl;
    int length = GetFreeListLength(head);
    std::cerr << "Length of this free list is now " << length << std::endl;
#endif
    return result;
}

/* Helper for the ideal SLLPop sequence.
 * Returns a SW prefetch to represent the best case cache-warming effects of the SLL pop.
 * Sets up an analysis routine to grab the correspoining memory operand. */
repl_vec_t GetIdealReplacements(const insn_vec_t& insns) {
    INS lzcnt = insns.front();
    REG head_reg = INS_RegR(lzcnt, 0);
    xed_reg_enum_t head_reg_xed = PinRegToXedReg(head_reg);

    INS_InsertCall(lzcnt,
                   IPOINT_BEFORE,
                   AFUNPTR(GetIdealMemOperands),
                   IARG_THREAD_ID,
                   IARG_INST_PTR,
                   IARG_REG_VALUE,
                   head_reg,
                   IARG_CALL_ORDER,
                   CALL_ORDER_FIRST,
                   IARG_END);

    repl_vec_t result;

    std::list<xed_encoder_instruction_t> lzcnt_repl;
    xed_encoder_instruction_t sw_pf;
    /* we'll give it a mem operand of *[head_reg], not just [head_reg] */
    xed_inst1(&sw_pf, dstate, XED_ICLASS_PREFETCHT0, 0,
              xed_mem_b(head_reg_xed, 64 * 8));
    lzcnt_repl.push_back(sw_pf);
    result.emplace_back(lzcnt_repl, true);
    return result;
}

void RegisterEmulation(INS ins) {
    INS_InsertCall(ins,
                   IPOINT_BEFORE,
                   AFUNPTR(Emulation),
                   IARG_REG_VALUE,
                   INS_RegR(ins, 0),
                   IARG_RETURN_REGS,
                   INS_RegW(ins, 0),
                   IARG_CALL_ORDER,
                   /* +1, so GetBaselineMemOperands gets operands *before* any emulation */
                   CALL_ORDER_FIRST + 1,
                   IARG_END);
}

/* Helper for the baseline SLL_Pop sequence.
 * Returns the xed-encoded list of instructions that make up SLL_Pop.
 */
repl_vec_t GetBaselineReplacements(const insn_vec_t& insns) {
    INS lzcnt = insns.front();
    REG head_reg = INS_RegR(lzcnt, 0);
    REG res_reg = INS_RegW(lzcnt, 0);
    repl_vec_t result;

    INS_InsertCall(lzcnt,
                   IPOINT_BEFORE,
                   AFUNPTR(GetBaselineMemOperands),
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

    result.emplace_back(repl, true);
    return result;
}

}  // namespace SLLPop

namespace SLLPush {

/* The placeholder instruction we compile with is a bextr dst, src1, src2.
 * src1 holds the address of the LL head, src2 holds the element we're pushing.
 * [src1] will point to src2, and [src2] will point to old [src1].
 * dst is ignored. */
static void Emulation(ADDRINT head, ADDRINT element) {
#ifdef EMULATION_DEBUG
    std::cerr << "Emulating SLLPush: head=0x" << std::hex << head;
#endif
    ADDRINT tmp;
    /* Get that LL head! */
    PIN_SafeCopy(&tmp, reinterpret_cast<VOID*>(head), sizeof(ADDRINT));
    /* Store that head in element! */
    PIN_SafeCopy(reinterpret_cast<VOID*>(element), &tmp, sizeof(ADDRINT));
    /* Look at me, I'm the head now */
    PIN_SafeCopy(reinterpret_cast<VOID*>(head), &element, sizeof(ADDRINT));
#ifdef EMULATION_DEBUG
    std::cerr << ", current_head=0x" << tmp << ", new head element=0x" << element << std::dec
              << "\n";
    int length = GetFreeListLength(head);
    // tcmalloc updates the length_ variable AFTER calling SLLPush, so print length + 1.
    std::cerr << "Length of this free list is now " << length + 1 << "\n" << std::endl;
#endif
}

/* Analysis routine to prepare memory addresses for a potential replacement. */
static void GetBaselineMemOperands(THREADID tid, ADDRINT pc, ADDRINT head, ADDRINT element) {
    thread_state_t* tstate = get_tls(tid);
    tstate->replacement_mem_ops[pc].clear();
    tstate->replacement_mem_ops.at(pc).push_back({head, sizeof(ADDRINT)});
    tstate->replacement_mem_ops.at(pc).push_back({element, sizeof(ADDRINT)});
    tstate->replacement_mem_ops.at(pc).push_back({head, sizeof(ADDRINT)});
}

void RegisterEmulation(INS ins) {
    INS_InsertCall(ins,
                   IPOINT_BEFORE,
                   AFUNPTR(Emulation),
                   IARG_REG_VALUE,
                   INS_RegR(ins, 0),
                   IARG_REG_VALUE,
                   INS_RegR(ins, 1),
                   IARG_CALL_ORDER,
                   /* +1, so GetMemOperands gets operands *before* any emulation */
                   CALL_ORDER_FIRST + 1,
                   IARG_END);
}

/* Helper for the baseline SLLPush sequence.
 * Returns the xed-encoded list of instructions that make up SLLPush.
 * Sets up an analysis routine to grab the correspoining memory operands. */
repl_vec_t GetBaselineReplacements(const insn_vec_t& insns) {
    INS bextr = insns.front();
    REG head_reg = INS_RegR(bextr, 0);
    REG element_reg = INS_RegR(bextr, 1);

    INS_InsertCall(bextr,
                   IPOINT_BEFORE,
                   AFUNPTR(GetBaselineMemOperands),
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

    repl_vec_t result;
    result.emplace_back(repl, true);
    return result;
}

}  // namespace SLLPush

namespace LLHeadCacheLookup {

/* Look for a head ptr for the given size class in the free list head cache.
 *
 * For baseline and ideal, we just return NULL (0) to force the producer to go
 * down the fallback path. For realistic, we actually perform the lookup.
 */
static ADDRINT Emulation(THREADID tid, ADDRINT cl) {
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return 0;
    switch (freelist_mode) {
    case BASELINE:
    case IDEAL:
        return 0;
    case REALISTIC: {
        thread_state_t* tstate = get_tls(tid);
        void* next_head;
        /* If success is false, return nullptr so that the test instruction
         * uses the fallback value. This is so we don't need to play with FLAGS
         * in the emulation routine.
         */
        bool success = tstate->size_class_cache.head_pop(cl, &next_head);
#ifdef EMULATION_DEBUG
        std::cerr << "Emulating LLHeadCacheLookup: class=" << cl << ", next_head=" << next_head
                  << ", success=" << success << std::endl;
#endif
        if (success)
            return reinterpret_cast<ADDRINT>(next_head);
        else
            return 0;
    }
    default:
        return 0;
    }
}

/* Pass input operands to the timing simulator. */
static VOID GetRegOperands(THREADID tid, ADDRINT cl) {
    switch (freelist_mode) {
    case BASELINE:
    case IDEAL:
        return;
    case REALISTIC: {
        if (ExecMode != EXECUTION_MODE_SIMULATE)
            return;
        thread_state_t* tstate = get_tls(tid);
        // Abusing mem_buffer to store the requested size class (input operand
        // of this magic instruction). Don't store the returned head - that is
        // an output operand.
        auto handshake = xiosim::buffer_management::GetBuffer(tstate->tid);
        handshake->mem_buffer.push_back(std::make_pair(0, static_cast<uint8_t>(cl)));
    }
    }
}

/* For the baseline free list head lookup, just ignore all three extra
 * instructions, since the fallback path will be executed anyways.
 */
repl_vec_t GetBaselineReplacements(const insn_vec_t& insns) {
    std::vector<magic_insn_action_t> result;
    // There is some non-determinism in how many instructions we actually
    // found (may not find the mov or the test), but that doesn't really
    // matter; we just want to ignore all of them.
    for (unsigned i = 0; i < insns.size(); i++) {
        result.emplace_back();
    }
    return result;
}

void RegisterEmulation(INS ins) {
    // We will delete the shrd on the host but leave the test instruction so
    // the host takes the right path.
    INS_InsertCall(ins,
                   IPOINT_BEFORE,
                   AFUNPTR(GetRegOperands),
                   IARG_THREAD_ID,
                   IARG_REG_VALUE,
                   INS_RegR(ins, 1),
                   IARG_CALL_ORDER,
                   CALL_ORDER_FIRST,
                   IARG_END);
    INS_InsertCall(ins,
                   IPOINT_BEFORE,
                   AFUNPTR(Emulation),
                   IARG_THREAD_ID,
                   IARG_REG_VALUE,
                   INS_RegR(ins, 1),
                   IARG_RETURN_REGS,
                   INS_RegW(ins, 0),
                   IARG_CALL_ORDER,
                   CALL_ORDER_FIRST + 1,
                   IARG_END);
}

/* The shrd instruction can't set a flag based on the result of the lookup,
 * because it's too far removed from the test, so the test can't be ignored.
 * We can ignore the mov instruction - that's just an artifact of using a three
 * operand placeholder instruction while ignoring the write operand.
 */
repl_vec_t GetRealisticReplacements(const insn_vec_t& insns) {
    ASSERTX(insns.size() >= 1 || insns.size() <= 4);
    std::list<xed_encoder_instruction_t> empty;
    std::vector<magic_insn_action_t> result;
    result.emplace_back(empty, false);  // Don't ignore shrd.
    for (unsigned i = 1; i < insns.size(); i++) {
        if (XED_INS_CATEGORY(insns[i]) == XED_CATEGORY_CMOV ||
            XED_INS_ICLASS(insns[i]) == XED_ICLASS_TEST)
            result.emplace_back(empty, false);
        else
            result.emplace_back();
    }
    return result;
}

/* Get the magic instruction sequence shrd; test; cmovcc, if possible.
 *
 * shrd and the other instructions are separated by the fallback code (which
 * must be executed regardless) and end up on different basic blocks, so this
 * must be called from IMG instrumentation, not TRACE.
 *
 * Depending on what the optimizer does, the other instructions may or may not
 * exist in the current routine.
 */
insn_vec_t LocateMagicSequence(const INS& ins) {
    const INS& shrd = ins;
    std::vector<INS> insns{ shrd };
    LEVEL_BASE::REG shrd_dest = INS_RegW(shrd, 0);
    INS next = INS_Next(shrd);
    while (INS_Valid(next)) {
        next = GetNextInsOfClass(next, XED_ICLASS_TEST);
        if (INS_RegRContain(next, shrd_dest)) {
            insns.push_back(next);
            break;
        }
        next = INS_Next(next);
    }
    INS cmov = GetNextInsOfCategory(next, XED_CATEGORY_CMOV);
    if (INS_Valid(cmov)) {
        insns.push_back(cmov);
    }
    return insns;
}

}  // namespace LLHeadCacheLookup

namespace LLHeadCacheUpdate {

/* Update the linked list head cache on the producer.
 *
 * Arguments:
 *   tid: Pin thread id.
 *   ptr: Either the current head of the free list or an element being returned
 *     to a free list. The way it is handled depends on get_next_head;
 *   cl: Size class of this pointer.
 *   get_next_head: If true, insert the value of *ptr (aka the NEXT head) into
 *     the cache, rather than the value of head itself. This is true for SLLPop
 *     and false for SLLPush.
 *
 * Returns:
 *   true if the update succeeded, false otherwise.
 */
static BOOL Emulation(THREADID tid, ADDRINT ptr, ADDRINT cl, BOOL get_next_head) {
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return false;
    switch (freelist_mode) {
    case BASELINE:
    case IDEAL:
        return false;
    case REALISTIC: {
        // If we're not simulating, none of these optimizations matter.
#ifdef EMULATION_DEBUG
        std::cerr << "Emulating LLHeadCacheUpdate: class=" << cl << ", head=0x" << std::hex << ptr;
#endif
        thread_state_t* tstate = get_tls(tid);
        ADDRINT next_head;
        if (get_next_head)
            PIN_SafeCopy(&next_head, reinterpret_cast<VOID*>(ptr), sizeof(ADDRINT));
        else
            next_head = ptr;
        bool success = tstate->size_class_cache.head_update(cl, reinterpret_cast<void*>(next_head));

#ifdef EMULATION_DEBUG
        std::cerr << ", next_head=0x" << next_head << ", success:" << success << std::dec << std::endl;
#endif
        return success;
    }
    default:
        return false;
    }
}


/* Pass input operands to the timing simulator. */
static VOID GetRegOperands(THREADID tid, ADDRINT head_ptr, ADDRINT cl, BOOL get_next_head) {
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;
    switch (freelist_mode) {
    case BASELINE:
    case IDEAL:
        return;
    case REALISTIC: {
        thread_state_t* tstate = get_tls(tid);
        ADDRINT next_head;
        if (get_next_head)
            PIN_SafeCopy(&next_head, reinterpret_cast<VOID*>(head_ptr), sizeof(ADDRINT));
        else
            next_head = head_ptr;
        auto handshake = xiosim::buffer_management::GetBuffer(tstate->tid);
        handshake->mem_buffer.push_back(
                std::make_pair(static_cast<md_addr_t>(next_head), static_cast<uint8_t>(cl)));
    }
    }
}

void RegisterEmulation(INS ins) {
    INS_InsertCall(ins,
                   IPOINT_BEFORE,
                   AFUNPTR(GetRegOperands),
                   IARG_THREAD_ID,
                   IARG_REG_VALUE,
                   INS_RegR(ins, 0),
                   IARG_REG_VALUE,
                   INS_RegR(ins, 1),
                   IARG_BOOL,
                   INS_IsMemoryRead(ins),
                   IARG_CALL_ORDER,
                   CALL_ORDER_FIRST,
                   IARG_END);
    INS_InsertCall(ins,
                   IPOINT_BEFORE,
                   AFUNPTR(Emulation),
                   IARG_THREAD_ID,
                   IARG_REG_VALUE,
                   INS_RegR(ins, 0),
                   IARG_REG_VALUE,
                   INS_RegR(ins, 1),
                   IARG_BOOL,
                   INS_IsMemoryRead(ins),
                   IARG_CALL_ORDER,
                   CALL_ORDER_FIRST + 1,
                   IARG_END);
}

/* Same for the baseline free list cache update. */
repl_vec_t GetBaselineReplacements(const insn_vec_t& insns) {
    std::vector<magic_insn_action_t> result;
    result.emplace_back();  // shrx
    return result;
}

}  // namespace LLHeadCacheUpdate
