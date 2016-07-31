#include <list>
#include <vector>

#include "xiosim/regs.h"
#include "xiosim/pintool/feeder.h"
#include "xiosim/pintool/xed_utils.h"

#include "tcm_utils.h"

namespace Sampling {

/*
 * Return value goes to RAX. In host execution, this is followed by a
 * test rax, rax; jne <PickNextSamplingPoint>, so non-zero means we'll
 * sample.
 * */
bool Sampling_Emulation(ADDRINT bytes_until_sample_addr, ADDRINT size) {
    ADDRINT tmp;
    PIN_SafeCopy(&tmp, reinterpret_cast<VOID*>(bytes_until_sample_addr), sizeof(ADDRINT));
    if (tmp >= size) {
        tmp -= size;
        PIN_SafeCopy(reinterpret_cast<VOID*>(bytes_until_sample_addr), &tmp, sizeof(ADDRINT));
        return 0;
    }
    return 1;
}

/* For sampling, the magic sequence we insert isn't a single instruction.
 * Grab the adc; lahf; test; jne; sequence that we'll be modifying.
 * They are all one the same bbl, so we can do this without gymnastics. */
insn_vec_t LocateMagicSequence(const INS& adc) {
    std::vector<INS> result{adc};

    INS lahf = INS_Next(adc);
    ASSERTX(INS_Valid(lahf));
    ASSERTX(XED_INS_ICLASS(lahf) == XED_ICLASS_LAHF);
    result.push_back(lahf);

    /* ... and let's make sure the lahf is always followed by the test and jump. */
    INS test = INS_Next(lahf);
    ASSERTX(INS_Valid(test));
    ASSERTX(XED_INS_ICLASS(test) == XED_ICLASS_TEST);
    result.push_back(test);

    INS jne = GetNextInsOfClass(test, XED_ICLASS_JNZ);
    result.push_back(jne);

    return result;
}

void RegisterEmulation(INS ins)  {
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
    auto insns = LocateMagicSequence(ins);
    INS lahf = insns[1];
    INS_Delete(lahf);
}

/* Analysis routine to prepare memory addresses for a potential replacement. */
void Sampling_GetMemOperands(THREADID tid, ADDRINT pc, ADDRINT jne_pc, ADDRINT addr) {
    thread_state_t* tstate = get_tls(tid);
    tstate->replacement_mem_ops[pc].clear();
    tstate->replacement_mem_ops.at(pc).push_back({addr, sizeof(ADDRINT)});

    tstate->replacement_mem_ops[jne_pc].clear();
    tstate->replacement_mem_ops.at(jne_pc).push_back({addr, sizeof(ADDRINT)});
}

repl_vec_t GetBaselineReplacements(const insn_vec_t& insns) {
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

}  // namespace Sampling
