#include <iostream>

#include "decode.h"

namespace xiosim {
namespace x86 {

xed_state_t machine_mode;

void init_decoder() {
    xed_tables_init();
    // XXX: Determine 32/64 from compiler flags
    xed_state_init2(&machine_mode, XED_MACHINE_MODE_LONG_COMPAT_32, XED_ADDRESS_WIDTH_32b);
}

void decode(struct Mop_t * Mop) {
    xed_decoded_inst_zero_set_mode(&Mop->decode.inst, &machine_mode);
    auto ret = xed_decode(&Mop->decode.inst, Mop->fetch.inst.code, MAX_ILEN);
    if (ret != XED_ERROR_NONE)
        fatal("XED failed to decode instruction at %x: ", Mop->fetch.PC);

    Mop->fetch.inst.len = xed_decoded_inst_get_length(&Mop->decode.inst);
}


bool is_trap(const struct Mop_t * Mop) {
    auto iclass = xed_decoded_inst_get_iclass(&Mop->decode.inst);
    switch (iclass) {
      case XED_ICLASS_CPUID:
      case XED_ICLASS_INT:
      case XED_ICLASS_INT1:
      case XED_ICLASS_INT3:
      case XED_ICLASS_HLT:
      // Other serializing instructions: flag accessors?
        return true;
      default:
        return false;
    }
    return false;
}

bool is_ctrl(const struct Mop_t * Mop) {
    auto icat = xed_decoded_inst_get_category(&Mop->decode.inst);
    switch (icat) {
      case XED_CATEGORY_CALL:
      case XED_CATEGORY_RET:
      case XED_CATEGORY_COND_BR:
      case XED_CATEGORY_UNCOND_BR:
        return true;
      default:
        return false;
    }
    return false;
}

bool is_load(const struct Mop_t * Mop) {
    size_t num_mem = xed_decoded_inst_number_of_memory_operands(&Mop->decode.inst);
    for (size_t i = 0; i < num_mem; i++)
        if (xed_decoded_inst_mem_read(&Mop->decode.inst, i))
            return true;
    return false;
}

bool is_store(const struct Mop_t * Mop) {
    size_t num_mem = xed_decoded_inst_number_of_memory_operands(&Mop->decode.inst);
    for (size_t i = 0; i < num_mem; i++)
        if (xed_decoded_inst_mem_written(&Mop->decode.inst, i))
            return true;
    return false;
}

bool is_nop(const struct Mop_t * Mop) {
    auto icat = xed_decoded_inst_get_category(&Mop->decode.inst);
    switch (icat) {
      case XED_CATEGORY_NOP:
      case XED_CATEGORY_WIDENOP:
        return true;
      default:
        return false;
    }
    return false;
}

bool is_fence(const struct Mop_t * Mop) {
    auto iclass = xed_decoded_inst_get_iclass(&Mop->decode.inst);
    switch (iclass) {
      case XED_ICLASS_MFENCE:
      case XED_ICLASS_SFENCE:
      case XED_ICLASS_LFENCE:
        return true;
      default:
        return false;
    }
    return false;
}

void decode_flags(struct Mop_t * Mop) {
    Mop->decode.is_trap = is_trap(Mop);
    Mop->decode.is_ctrl = is_ctrl(Mop);
}

std::string print_Mop(const struct Mop_t * Mop) {
    char buffer[255];

    xed_decoded_inst_dump(&Mop->decode.inst, buffer, sizeof(buffer));
    bool ok = xed_format_context(XED_SYNTAX_INTEL, &Mop->decode.inst, buffer, sizeof(buffer), 0, 0, 0);
    if (!ok)
        return "ERROR dissasembling";
    return buffer;
}

}
}
