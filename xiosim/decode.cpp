#include "misc.h"
#include "decode.h"
#include "zesto-structs.h"

namespace xiosim {
namespace x86 {

static xed_state_t machine_mode;

void init_decoder() {
    xed_tables_init();
#ifdef _LP64
    xed_state_init2(&machine_mode, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b);
#else
    xed_state_init2(&machine_mode, XED_MACHINE_MODE_LONG_COMPAT_32, XED_ADDRESS_WIDTH_32b);
#endif
}

void decode(struct Mop_t * Mop) {
    xed_decoded_inst_zero_set_mode(&Mop->decode.inst, &machine_mode);
    auto ret = xed_decode(&Mop->decode.inst, Mop->fetch.code, MAX_ILEN);
    if (ret != XED_ERROR_NONE)
        fatal("XED failed to decode instruction at %x: ", Mop->fetch.PC);

    Mop->fetch.len = xed_decoded_inst_get_length(&Mop->decode.inst);
}


bool is_trap(const struct Mop_t * Mop) {
    auto iclass = xed_decoded_inst_get_iclass(&Mop->decode.inst);
    switch (iclass) {
      case XED_ICLASS_CPUID:
      case XED_ICLASS_INT:
      case XED_ICLASS_INT1:
      case XED_ICLASS_INT3:
      case XED_ICLASS_HLT:
      case XED_ICLASS_SYSCALL:
      case XED_ICLASS_SYSCALL_AMD:
      case XED_ICLASS_SYSENTER:
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

bool is_indirect(const struct Mop_t * Mop) {
    auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
    switch (iform) {
      case XED_IFORM_CALL_FAR_MEMp2:
      case XED_IFORM_CALL_FAR_PTRp_IMMw:
      case XED_IFORM_CALL_NEAR_GPRv:
      case XED_IFORM_CALL_NEAR_MEMv:
      case XED_IFORM_JMP_GPRv:
      case XED_IFORM_JMP_MEMv:
      case XED_IFORM_JMP_FAR_MEMp2:
      case XED_IFORM_JMP_FAR_PTRp_IMMw:
        return true;
      default:
        return false;
    }
    return false;
}

bool is_fp(const struct Mop_t * Mop) {
    auto icat = xed_decoded_inst_get_category(&Mop->decode.inst);
    switch (icat) {
      case XED_CATEGORY_3DNOW:
      case XED_CATEGORY_AVX:
      case XED_CATEGORY_AVX2:
      case XED_CATEGORY_AVX2GATHER:
      case XED_CATEGORY_MMX:
      case XED_CATEGORY_SSE:
      case XED_CATEGORY_X87_ALU:
        return true;
      default:
        return false;
    }
    return false;
}

void decode_flags(struct Mop_t * Mop) {
    Mop->decode.is_trap = is_trap(Mop);
    Mop->decode.is_ctrl = is_ctrl(Mop);

    auto icat = xed_decoded_inst_get_category(&Mop->decode.inst);
    inst_flags_t& flags = Mop->decode.opflags;
    flags.CTRL = Mop->decode.is_ctrl;
    flags.CALL = (icat == XED_CATEGORY_CALL);
    flags.RETN = (icat == XED_CATEGORY_RET);
    flags.COND = (icat == XED_CATEGORY_COND_BR);
    flags.UNCOND = (icat == XED_CATEGORY_UNCOND_BR) ||
                   flags.CALL || flags.RETN;
    flags.LOAD = is_load(Mop);
    flags.STORE = is_store(Mop);
    flags.MEM = flags.LOAD || flags.STORE;
    flags.TRAP = Mop->decode.is_trap;
    flags.INDIR = is_indirect(Mop);
}

std::string print_Mop(const struct Mop_t * Mop) {
    char buffer[511];

    xed_decoded_inst_dump(&Mop->decode.inst, buffer, sizeof(buffer));
    bool ok = xed_format_context(XED_SYNTAX_INTEL, &Mop->decode.inst, buffer, sizeof(buffer), 0, 0, 0);
    if (!ok)
        return "ERROR dissasembling";
    return buffer;
}

xed_iclass_enum_t xed_iclass(const struct Mop_t * Mop) {
    return xed_decoded_inst_get_iclass(&Mop->decode.inst);
}

}  // xiosim::x86
}  // xiosim
