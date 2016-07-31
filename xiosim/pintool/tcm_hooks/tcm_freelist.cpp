#include <list>
#include <vector>

#include "xiosim/regs.h"
#include "xiosim/pintool/feeder.h"
#include "xiosim/pintool/xed_utils.h"

#include "tcm_utils.h"

namespace SLLPop {

/* Analysis routine to get the next head to prefetch. */
void SLLPop_GetIdealMemOperands(THREADID tid, ADDRINT pc, ADDRINT head) {
    ADDRINT result;
    /* Dereference the LL head. */
    PIN_SafeCopy(&result, reinterpret_cast<VOID*>(head), sizeof(ADDRINT));

    thread_state_t* tstate = get_tls(tid);
    tstate->replacement_mem_ops[pc].clear();
    tstate->replacement_mem_ops.at(pc).push_back({result, sizeof(ADDRINT)});
}

/* Analysis routine to prepare memory addresses for a potential replacement. */
void SLLPop_GetBaselineMemOperands(THREADID tid, ADDRINT pc, ADDRINT head) {
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
ADDRINT SLLPop_Emulation(ADDRINT head) {
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


/* Helper for the ideal SLLPop sequence.
 * Returns a SW prefetch to represent the best case cache-warming effects of the SLL pop.
 * Sets up an analysis routine to grab the correspoining memory operand. */
repl_vec_t GetIdealReplacements(const insn_vec_t& insns) {
    INS lzcnt = insns.front();
    REG head_reg = INS_RegR(lzcnt, 0);
    xed_reg_enum_t head_reg_xed = PinRegToXedReg(head_reg);

    INS_InsertCall(lzcnt,
                   IPOINT_BEFORE,
                   AFUNPTR(SLLPop_GetIdealMemOperands),
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
                   AFUNPTR(SLLPop_Emulation),
                   IARG_REG_VALUE,
                   INS_RegR(ins, 0),
                   IARG_RETURN_REGS,
                   INS_RegW(ins, 0),
                   IARG_CALL_ORDER,
                   /* +1, so SLLPop_GetBaselineMemOperands gets operands *before* any emulation */
                   CALL_ORDER_FIRST + 1,
                   IARG_END);
}

/* Helper for the baseline SLLPop sequence.
 * Returns the xed-encoded list of instructions that make up SLLPop.
 * Sets up an analysis routine to grab the correspoining memory operands. */
repl_vec_t GetBaselineReplacements(const insn_vec_t& insns) {
    INS lzcnt = insns.front();
    REG head_reg = INS_RegR(lzcnt, 0);
    REG res_reg = INS_RegW(lzcnt, 0);

    INS_InsertCall(lzcnt,
                   IPOINT_BEFORE,
                   AFUNPTR(SLLPop_GetBaselineMemOperands),
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

    repl_vec_t result;
    result.emplace_back(repl, true);
    return result;
}

}  // namespace SLLPop

namespace SLLPush {

/* The placeholder instruction we compile with is a bextr dst, src1, src2.
 * src1 holds the address of the LL head, src2 holds the element we're pushing.
 * [src1] will point to src2, and [src2] will point to old [src1].
 * dst is ignored. */
void SLLPush_Emulation(ADDRINT head, ADDRINT element) {
    ADDRINT tmp;
    /* Get that LL head! */
    PIN_SafeCopy(&tmp, reinterpret_cast<VOID*>(head), sizeof(ADDRINT));
    /* Store that head in element! */
    PIN_SafeCopy(reinterpret_cast<VOID*>(element), &tmp, sizeof(ADDRINT));
    /* Look at me, I'm the head now */
    PIN_SafeCopy(reinterpret_cast<VOID*>(head), &element, sizeof(ADDRINT));
}

/* Analysis routine to prepare memory addresses for a potential replacement. */
void SLLPush_GetMemOperands(THREADID tid, ADDRINT pc, ADDRINT head, ADDRINT element) {
    thread_state_t* tstate = get_tls(tid);
    tstate->replacement_mem_ops[pc].clear();
    tstate->replacement_mem_ops.at(pc).push_back({head, sizeof(ADDRINT)});
    tstate->replacement_mem_ops.at(pc).push_back({element, sizeof(ADDRINT)});
    tstate->replacement_mem_ops.at(pc).push_back({head, sizeof(ADDRINT)});
}

void RegisterEmulation(INS ins) {
    INS_InsertCall(ins,
                   IPOINT_BEFORE,
                   AFUNPTR(SLLPush_Emulation),
                   IARG_REG_VALUE,
                   INS_RegR(ins, 0),
                   IARG_REG_VALUE,
                   INS_RegR(ins, 1),
                   IARG_CALL_ORDER,
                   /* +1, so SLLPush_GetMemOperands gets operands *before* any emulation */
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
                   AFUNPTR(SLLPush_GetMemOperands),
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
