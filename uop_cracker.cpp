#include <cstddef>
#include <iostream>
#include <list>

#include "uop_cracker.h"

using namespace std;

namespace xiosim {
namespace x86 {

static list<xed_reg_enum_t> get_registers_read(const struct Mop_t * Mop);
static list<xed_reg_enum_t> get_registers_written(const struct Mop_t * Mop);

static struct uop_t * get_uop_array(const size_t size)
{
    void *space = nullptr;
    /* Allocate aligned storage for the uop array. */
    if (posix_memalign(&space, alignof(struct uop_t), size * sizeof(struct uop_t)))
        fatal("Memory allocation failed.");

    return new (space) uop_t[size];
}

void clear_uop_array(struct Mop_t * Mop)
{
    delete []Mop->uop;
    Mop->uop = nullptr;
}

/* Create a 1-uop mapping for this Mop, keeping:
 * - register input / output same as the Mop
 * - flags (load/store/jump) same as the Mop
 */
static void fallback(struct Mop_t * Mop) {
    /* Grab number of uops. We assume 1 uop, plus 1 for a potential load,
     * and 2 more for a potential store (STA and STD). */
    size_t op_index = 0;
    Mop->decode.flow_length = 1;

    bool Mop_has_load = is_load(Mop);
    if (Mop_has_load) {
        op_index = Mop->decode.flow_length;
        Mop->decode.flow_length += 1;
    }

    bool Mop_has_store = is_store(Mop);
    if (Mop_has_store) {
        Mop->decode.flow_length += 2;
    }

    /* Allocate uop array that we can start filling out. */
    Mop->uop = get_uop_array(Mop->decode.flow_length);

    /* Fill out flags for main uop. */
    Mop->uop[op_index].decode.is_ctrl = Mop->decode.is_ctrl;
    Mop->uop[op_index].decode.is_nop = is_nop(Mop);
    Mop->uop[op_index].decode.is_fence = is_fence(Mop);

    /* Fill out register read dependences from XED.
     * All of them go to the main uop. */
    auto ideps = get_registers_read(Mop);
    assert(ideps.size() <= MAX_IDEPS);
    /* Reserve idep 0 for the load->op dependence through a temp register. */
    int idep_ind = Mop_has_load ? 1 : 0;
    for (auto it = ideps.begin(); it != ideps.end(); it++, idep_ind++) {
        Mop->uop[op_index].decode.idep_name[idep_ind] = *it;
    }

    /* Similarly for register write dependences. */
    auto odeps = get_registers_written(Mop);
    assert(odeps.size() <= MAX_ODEPS);
    /* Reserve odep 0 for the op->store dependence through a temp register. */
    int odep_ind = Mop_has_store ? 1 : 0;
    for (auto it = odeps.begin(); it != odeps.end(); it++, odep_ind++) {
        Mop->uop[op_index].decode.odep_name[odep_ind] = *it;
    }

    /* First uop is a load, and generates an address to a temp register. */
    if (Mop_has_load) {
        Mop->uop[0].decode.is_load = true;
        Mop->uop[0].decode.odep_name[0] = XED_REG_TMP0;

        assert(Mop->uop[op_index].decode.idep_name[0] == XED_REG_INVALID);
        Mop->uop[op_index].decode.idep_name[0] = XED_REG_TMP0;
        /* TODO(skanev): Add implicit register dependences for address generation. */
    }

    /* Last two uops are STA and STD. */
    if (Mop_has_store) {
        auto std_temp_reg = Mop_has_load ? XED_REG_TMP1 : XED_REG_TMP0;
        assert(Mop->uop[op_index].decode.odep_name[0] == XED_REG_INVALID);
        Mop->uop[op_index].decode.odep_name[0] = std_temp_reg;

        Mop->uop[op_index+1].decode.is_sta = true;
        /* TODO(skanev): Add implicit register dependences for address generation. */

        Mop->uop[op_index+2].decode.is_std = true;
        Mop->uop[op_index+2].decode.idep_name[0] = std_temp_reg;
    }
}

static bool check_load(const struct Mop_t * Mop) {
    auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
    switch (iform) {
      case XED_IFORM_MOV_GPR8_MEMb:
      case XED_IFORM_MOV_GPRv_MEMv:
      case XED_IFORM_MOV_OrAX_MEMv:
      case XED_IFORM_MOVAPD_XMMpd_MEMpd:
      case XED_IFORM_MOVAPS_XMMps_MEMps:
      case XED_IFORM_MOVBE_GPRv_MEMv:
      case XED_IFORM_MOVD_MMXq_MEMd:
      case XED_IFORM_MOVD_XMMdq_MEMd:
      case XED_IFORM_MOVDDUP_XMMdq_MEMq:
      case XED_IFORM_MOVDQA_XMMdq_MEMdq:
      case XED_IFORM_MOVDQU_XMMdq_MEMdq:
      case XED_IFORM_MOVHPD_XMMsd_MEMq:
      case XED_IFORM_MOVHPS_XMMq_MEMq:
      case XED_IFORM_MOVLPD_XMMsd_MEMq:
      case XED_IFORM_MOVLPS_XMMq_MEMq:
      case XED_IFORM_MOVNTDQA_XMMdq_MEMdq:
      case XED_IFORM_MOVQ_MMXq_MEMq_0F6E:
      case XED_IFORM_MOVQ_MMXq_MEMq_0F6F:
      case XED_IFORM_MOVQ_XMMdq_MEMq_0F6E:
      case XED_IFORM_MOVQ_XMMdq_MEMq_0F7E:
      case XED_IFORM_MOVSD_XMM_MEMsd_XMMsd:
      case XED_IFORM_MOVSHDUP_XMMps_MEMps:
      case XED_IFORM_MOVSLDUP_XMMps_MEMps:
      case XED_IFORM_MOVSS_XMMdq_MEMss:
      case XED_IFORM_MOVSX_GPRv_MEMb:
      case XED_IFORM_MOVSX_GPRv_MEMw:
      case XED_IFORM_MOVSXD_GPRv_MEMd:
      case XED_IFORM_MOVUPD_XMMpd_MEMpd:
      case XED_IFORM_MOVUPS_XMMps_MEMps:
      case XED_IFORM_MOVZX_GPRv_MEMb:
      case XED_IFORM_MOVZX_GPRv_MEMw:
      case XED_IFORM_PMOVSXBD_XMMdq_MEMd:
      case XED_IFORM_PMOVSXBQ_XMMdq_MEMw:
      case XED_IFORM_PMOVSXBW_XMMdq_MEMq:
      case XED_IFORM_PMOVSXDQ_XMMdq_MEMq:
      case XED_IFORM_PMOVSXWD_XMMdq_MEMq:
      case XED_IFORM_PMOVSXWQ_XMMdq_MEMd:
      case XED_IFORM_PMOVZXBD_XMMdq_MEMd:
      case XED_IFORM_PMOVZXBQ_XMMdq_MEMw:
      case XED_IFORM_PMOVZXBW_XMMdq_MEMq:
      case XED_IFORM_PMOVZXDQ_XMMdq_MEMq:
      case XED_IFORM_PMOVZXWD_XMMdq_MEMq:
      case XED_IFORM_PMOVZXWQ_XMMdq_MEMd:
        return true;
      default:
        return false;
    }
    return false;
}

static bool check_store(const struct Mop_t * Mop) {
    auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
    switch (iform) {
      case XED_IFORM_MOV_MEMb_AL:
      case XED_IFORM_MOV_MEMb_GPR8:
      case XED_IFORM_MOV_MEMb_IMMb:
      case XED_IFORM_MOV_MEMv_GPRv:
      case XED_IFORM_MOV_MEMv_IMMz:
      case XED_IFORM_MOV_MEMv_OrAX:
      case XED_IFORM_MOV_MEMw_SEG:
      case XED_IFORM_MOVAPD_MEMpd_XMMpd:
      case XED_IFORM_MOVAPS_MEMps_XMMps:
      case XED_IFORM_MOVBE_MEMv_GPRv:
      case XED_IFORM_MOVD_MEMd_MMXd:
      case XED_IFORM_MOVD_MEMd_XMMd:
      case XED_IFORM_MOVDQA_MEMdq_XMMdq:
      case XED_IFORM_MOVDQU_MEMdq_XMMdq:
      case XED_IFORM_MOVHPD_MEMq_XMMsd:
      case XED_IFORM_MOVHPS_MEMq_XMMps:
      case XED_IFORM_MOVLPD_MEMq_XMMsd:
      case XED_IFORM_MOVLPS_MEMq_XMMps:
      case XED_IFORM_MOVNTDQ_MEMdq_XMMdq:
      case XED_IFORM_MOVNTI_MEMd_GPR32:
      case XED_IFORM_MOVNTI_MEMq_GPR64:
      case XED_IFORM_MOVNTPD_MEMdq_XMMpd:
      case XED_IFORM_MOVNTPS_MEMdq_XMMps:
      case XED_IFORM_MOVNTQ_MEMq_MMXq:
      case XED_IFORM_MOVNTSD_MEMq_XMMq:
      case XED_IFORM_MOVNTSS_MEMd_XMMd:
      case XED_IFORM_MOVQ_MEMq_MMXq_0F7E:
      case XED_IFORM_MOVQ_MEMq_MMXq_0F7F:
      case XED_IFORM_MOVQ_MEMq_XMMq_0F7E:
      case XED_IFORM_MOVQ_MEMq_XMMq_0FD6:
      case XED_IFORM_MOVUPD_MEMpd_XMMpd:
      case XED_IFORM_MOVUPS_MEMps_XMMps:
        return true;
      default:
        return false;
    }
    return false;
}

static bool check_call(const struct Mop_t * Mop) {
    auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
    switch (iform) {
      case XED_IFORM_CALL_NEAR_GPRv:
      case XED_IFORM_CALL_NEAR_MEMv:
      case XED_IFORM_CALL_NEAR_RELBRd:
      case XED_IFORM_CALL_NEAR_RELBRz:
        return true;
      default:
        return false;
    }
    return false;
}

static bool check_push(const struct Mop_t * Mop) {
    auto iclass = xed_decoded_inst_get_iclass(&Mop->decode.inst);
    switch (iclass) {
      case XED_ICLASS_PUSH:
      case XED_ICLASS_PUSHA:
      case XED_ICLASS_PUSHAD:
      case XED_ICLASS_PUSHF:
      case XED_ICLASS_PUSHFD:
      case XED_ICLASS_PUSHFQ:
        return true;
      default:
        return false;
    }
    
    return false;
}

static bool check_pop(const struct Mop_t * Mop) {
    auto iclass = xed_decoded_inst_get_iclass(&Mop->decode.inst);
    switch (iclass) {
      case XED_ICLASS_POP:
      case XED_ICLASS_POPA:
      case XED_ICLASS_POPAD:
      case XED_ICLASS_POPF:
      case XED_ICLASS_POPFD:
      case XED_ICLASS_POPFQ:
        return true;
      default:
        return false;
    }
    
    return false;
}

/* Check for MUL and the IMUL variants that have two
 * output registers (rAX:rDX).
 * The other variants can be handled by the fallback path. */
static bool check_mul(const struct Mop_t * Mop) {
    auto iclass = xed_decoded_inst_get_iclass(&Mop->decode.inst);
    switch (iclass) {
      case XED_ICLASS_MUL:
        return true;
      default:
        break;
    }

    auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
    switch (iform) {
      /* XED_IFORM_IMUL_GPR8 actually just writes AX. Sigh. */
      case XED_IFORM_IMUL_GPRv:
        return true;
      default:
        return false;
    }
    return false;
}

/* Check for the DIV and IDIV variants that have two
 * output registers (rAX and rDX).
 * The other variants can be handled by the fallback path. */
static bool check_div(const struct Mop_t * Mop) {
    auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
    switch (iform) {
      /* All 8-bit variants actually just write AX. Sigh. */
      case XED_IFORM_DIV_GPRv:
      case XED_IFORM_DIV_MEMv:
      case XED_IFORM_IDIV_GPRv:
      case XED_IFORM_IDIV_MEMv:
        return true;
      default:
        return false;
    }
    return false;
}

static bool check_cpuid(const struct Mop_t * Mop) {
    auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
    switch (iform) {
      case XED_IFORM_CPUID:
        return true;
      default:
        return false;
    }

    return false;
}

/* Check special-casing Mop->uop tables. Returns
 * true and modifies Mop if it has found a cracking.
 */
static bool check_tables(struct Mop_t * Mop) {
    if (check_load(Mop)) {
        Mop->decode.flow_length = 1;
        Mop->uop = get_uop_array(Mop->decode.flow_length);

        Mop->uop[0].decode.is_load = true;
        /* TODO(skanev): Add implicit register dependences for address generation. */
        return true;
    }

    if (check_store(Mop)) {
        Mop->decode.flow_length = 2;
        Mop->uop = get_uop_array(Mop->decode.flow_length);

        Mop->uop[0].decode.is_sta = true;
        /* TODO(skanev): Add implicit register dependences for address generation. */
        Mop->uop[1].decode.is_std = true;
        return true;
    }

    if (check_call(Mop)) {
        Mop->decode.flow_length = 4;
        Mop->uop = get_uop_array(Mop->decode.flow_length);

        Mop->uop[0].decode.is_sta = true;
        /* TODO(skanev): Add implicit register dependences for address generation. */
        Mop->uop[1].decode.is_std = true;
        // 2: Add to ESP
        Mop->uop[3].decode.is_ctrl = true;
        return true;
    }

    // XXX
    if (check_push(Mop) || check_pop(Mop)) {
        Mop->decode.flow_length = 1;
        Mop->uop = get_uop_array(Mop->decode.flow_length);
        return true;
    }

    /* Some (I)MUL and (I)DIV variants write to two output registers. */
    if (check_mul(Mop) || check_div(Mop)) {
        Mop->decode.flow_length = 3;
        Mop->uop = get_uop_array(Mop->decode.flow_length);
        //XXX: uop[0].idep
        auto ideps = get_registers_read(Mop);
        assert(ideps.size() <= MAX_IDEPS);
        /* Reserve idep 0 for the load->op dependence through a temp register. */
        int idep_ind = 0;
        for (auto it = ideps.begin(); it != ideps.end(); it++, idep_ind++) {
            Mop->uop[0].decode.idep_name[idep_ind] = *it;
        }

        //Mop->uop[0].decode.idep_name[0] = XED_REG_EAX;
        Mop->uop[0].decode.odep_name[0] = XED_REG_TMP0;
        Mop->uop[1].decode.idep_name[0] = XED_REG_TMP0;
        Mop->uop[1].decode.odep_name[0] = XED_REG_EAX;
        Mop->uop[2].decode.idep_name[0] = XED_REG_TMP0;
        Mop->uop[2].decode.odep_name[0] = XED_REG_EDX;
        return true;
    }

    /* CPUID writes to four output registers. */
    if (check_cpuid(Mop)) {
        Mop->decode.flow_length = 4;
        Mop->uop = get_uop_array(Mop->decode.flow_length);

        Mop->uop[0].decode.odep_name[0] = XED_REG_EAX;
        Mop->uop[1].decode.odep_name[0] = XED_REG_EBX;
        Mop->uop[2].decode.odep_name[0] = XED_REG_ECX;
        Mop->uop[3].decode.odep_name[0] = XED_REG_EDX;
        return true;
    }

    return false;
}

#define DEBUG_CRACKER
void crack(struct Mop_t * Mop) {
#ifdef DEBUG_CRACKER
    auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
    std::cerr << xed_iform_enum_t2str(iform) << std::endl;
#endif

    bool cracked = check_tables(Mop);
    if (cracked)
        return;
    /* Instead of fully describing Mop->uop tables,
     * we'll try and cover some of the simple common patterns (e.g. LOAD-OP-STORE).
     * We obviously need a mechanism for exceptions for non-standard (e.g. microcoded) ops.
     */
    fallback(Mop);

    /* there better be at least one uop */
    assert(Mop->decode.flow_length);
}

/* Get an instruction's expllicitly read registers.
 */
static list<xed_reg_enum_t> get_registers_read(const struct Mop_t * Mop) {
    list<xed_reg_enum_t> res;
    auto inst = xed_decoded_inst_inst(&Mop->decode.inst);
    for (size_t i = 0; i < xed_inst_noperands(inst); i++) {
        auto op = xed_inst_operand(inst, i);
#ifdef DEBUG_CRACKER
        char buf[256];
        xed_operand_print(op, buf, sizeof(buf));
        std::cerr << buf << std::endl;
#endif

        auto op_t = xed_operand_name(op);
        if (xed_operand_is_register(op_t) && xed_operand_read(op)) {
            xed_reg_enum_t reg = xed_decoded_inst_get_reg(&Mop->decode.inst, op_t);
            /* Let's not deal with partial register reads for now:
             * If we depend on part of the register, we depend on the largest version. */
            xed_reg_enum_t largest_reg = xed_get_largest_enclosing_register32(reg);
            res.push_back(largest_reg);

#ifdef DEBUG_CRACKER
            std::cerr << xed_reg_enum_t2str(reg) << std::endl;
            std::cerr << xed_reg_enum_t2str(largest_reg) << std::endl;
#endif
        }
    }
#ifdef DEBUG_CRACKER
    std::cerr << std::endl;
#endif
    return res;
}

/* Get an instruction's expllicitly written registers.
 */
static list<xed_reg_enum_t> get_registers_written(const struct Mop_t * Mop) {
    list<xed_reg_enum_t> res;

    auto inst = xed_decoded_inst_inst(&Mop->decode.inst);
    for (size_t i = 0; i < xed_inst_noperands(inst); i++) {
        auto op = xed_inst_operand(inst, i);

        auto op_t = xed_operand_name(op);
        if (xed_operand_is_register(op_t) && xed_operand_written(op)) {
            xed_reg_enum_t reg = xed_decoded_inst_get_reg(&Mop->decode.inst, op_t);
            /* Let's not deal with partial register reads for now:
             * If we depend on part of the register, we depend on the largest version. */
            xed_reg_enum_t largest_reg = xed_get_largest_enclosing_register32(reg);
            res.push_back(largest_reg);

#ifdef DEBUG_CRACKER
            std::cerr << xed_reg_enum_t2str(reg) << std::endl;
            std::cerr << xed_reg_enum_t2str(largest_reg) << std::endl;
#endif
        }
    }
    return res;
}

}
}

