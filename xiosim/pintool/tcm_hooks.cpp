#include <cctype>
#include <iomanip>
#include <iostream>
#include <string>

#include "xiosim/regs.h"

#include "BufferManagerProducer.h"
#include "feeder.h"
#include "replace_function.h"

#include "tcm_hooks.h"

KNOB<BOOL> KnobTCMHooks(KNOB_MODE_WRITEONCE, "pintool", "tcm_hooks", "true",
                        "Emulate tcmalloc replacements.");
KNOB<BOOL> KnobSLLPopMagic(KNOB_MODE_WRITEONCE, "pintool", "sll_pop_magic", "true",
                           "Simulate the magic lzcnt instruction for SLL_Pop.");
KNOB<BOOL> KnobSLLPopReal(KNOB_MODE_WRITEONCE, "pintool", "sll_pop_real", "false",
                          "Replace the lzcnt instruction with its baseline implementation.");

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
static std::list<xed_encoder_instruction_t> Prepare_SLL_PopSimulation(INS ins) {
    REG head_reg = INS_RegR(ins, 0);
    REG res_reg = INS_RegW(ins, 0);

    INS_InsertCall(ins,
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

    std::list<xed_encoder_instruction_t> result;
    xed_encoder_instruction_t get_head;
    xed_inst2(&get_head, dstate, XED_ICLASS_MOV, xed_mem_op_width,
              xed_reg(res_reg_xed),
              xed_mem_b(head_reg_xed, xed_mem_op_width));
    result.push_back(get_head);

    xed_encoder_instruction_t get_next;
    xed_inst2(&get_next, dstate, XED_ICLASS_MOV, xed_mem_op_width,
              /* We really want a temp register here, but don't know
               * what's available at instrumentation time. */
              xed_reg(largest_reg(XED_REG_R15D)),
              xed_mem_b(res_reg_xed, xed_mem_op_width));
    result.push_back(get_next);

    xed_encoder_instruction_t update_head;
    xed_inst2(&update_head, dstate, XED_ICLASS_MOV, xed_mem_op_width,
              xed_mem_b(head_reg_xed, xed_mem_op_width),
              xed_reg(largest_reg(XED_REG_R15D)));
    result.push_back(update_head);
    return result;
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
std::list<xed_encoder_instruction_t> Prepare_SLL_PushSimulation(INS ins) {
    REG head_reg = INS_RegR(ins, 0);
    REG element_reg = INS_RegR(ins, 1);

    INS_InsertCall(ins,
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

    std::list<xed_encoder_instruction_t> result;
    xed_encoder_instruction_t get_head;
    xed_inst2(&get_head, dstate, XED_ICLASS_MOV, xed_mem_op_width,
              /* We really want a temp register here, but don't know
               * what's available at instrumentation time. */
              xed_reg(largest_reg(XED_REG_R15D)),
              xed_mem_b(head_reg_xed, xed_mem_op_width));
    result.push_back(get_head);

    xed_encoder_instruction_t store_head;
    xed_inst2(&store_head, dstate, XED_ICLASS_MOV, xed_mem_op_width,
              xed_mem_b(element_reg_xed, xed_mem_op_width),
              xed_reg(largest_reg(XED_REG_R15D)));
    result.push_back(store_head);

    xed_encoder_instruction_t update_head;
    xed_inst2(&update_head, dstate, XED_ICLASS_MOV, xed_mem_op_width,
              xed_mem_b(head_reg_xed, xed_mem_op_width),
              xed_reg(element_reg_xed));
    result.push_back(update_head);
    return result;
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

static void InsertEmulationCode(INS ins, xed_iclass_enum_t iclass) {
    /* Add emulation regardless of whether we're simulating or not, so we
     * can preserve program semantics. */
    switch (iclass) {
    case XED_ICLASS_LZCNT:
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
    case XED_ICLASS_BEXTR:
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
    default:
        break;
    }

    /* Delete placeholder ins on the host side, so we don't get side effects from it. */
    INS_Delete(ins);
}

void InstrumentTCMHooks(TRACE trace, VOID* v) {
    if (!KnobTCMHooks.Value())
        return;

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            xed_iclass_enum_t iclass = static_cast<xed_iclass_enum_t>(INS_Opcode(ins));
            if (iclass != XED_ICLASS_LZCNT && iclass != XED_ICLASS_BEXTR)
                continue;

            SEC sec = RTN_Sec(INS_Rtn(ins));
            std::string sec_name = SEC_Name(sec);

            /* lzcnt/bextr don't naturally occur in google_malloc, so any instance we
             * see is fake and should be replaced. */
            if (sec_name != "google_malloc")
                continue;

#ifdef TCM_DEBUG
            std::cerr << "Found placeholder @ pc: " << std::hex << INS_Address(ins) << std::dec << std::endl;
#endif
            InsertEmulationCode(ins, iclass);

            /* Only emulation should run while we're fast-forwarding. */
            if (ExecMode != EXECUTION_MODE_SIMULATE)
                continue;

            if (KnobSLLPopMagic.Value()) {
                /* When we're simulating, mark the placeholder instruction as magic, so
                 * the timing simulator can evaluate performance. */
                INS_InsertCall(ins,
                               IPOINT_BEFORE,
                               AFUNPTR(MarkMagicInstruction),
                               IARG_THREAD_ID,
                               IARG_INST_PTR,
                               IARG_CALL_ORDER,
                               CALL_ORDER_FIRST + 2,
                               IARG_END);
            } else {
                std::list<xed_encoder_instruction_t> repl;
                if (KnobSLLPopReal.Value()) {
                    switch (iclass) {
                    case XED_ICLASS_LZCNT:
                        repl = Prepare_SLL_PopSimulation(ins);
                        break;
                    case XED_ICLASS_BEXTR:
                        repl = Prepare_SLL_PushSimulation(ins);
                        break;
                    default:
                        break;
                    }
                }
                AddInstructionReplacement(ins, repl);
            }
        }
    }
}
