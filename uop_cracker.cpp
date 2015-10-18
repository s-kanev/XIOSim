#include <cstddef>
#include <iostream>
#include <list>

#include "decode.h"
#include "uop_cracker.h"

using namespace std;

namespace xiosim {
namespace x86 {

/*
 * Dedicated pool for allocating uops.
 * We do need to keep our own free lists, as opposed to using a general-purpose allocator.
 * This is because we use action_id-s to figure out whether a uop is still valid.
 * In structures like caches or ALUs we only keep a pointer to the uop, and check the
 * action_id to see whether it has been cancelled. So, for speculative uops, we technically
 * access their storage after free-ing them. It's not ok to reclaim that space for
 * allocating anything that's not a uop. Note that it's ok to use it for uops because
 * the action_id field will still be at the same offset, and every structure will
 * drop the cancelled uop once it sees a mismatch.
 */
static thread_local std::list<uop_t*> uop_free_lists[MAX_NUM_UOPS + 1];

struct uop_t* get_uop_array(const size_t num_uops) {
    assert(num_uops > 0 && num_uops <= MAX_NUM_UOPS);
    auto& free_list = uop_free_lists[num_uops];

    void* space = nullptr;
    if (free_list.empty()) {
        /* Allocate aligned storage for the uop array. */
        if (posix_memalign(&space, alignof(struct uop_t), num_uops * sizeof(struct uop_t)))
            fatal("Memory allocation failed.");
    } else {
        /* We have an appropriate free list entry for reuse. */
        space = free_list.front();
        free_list.pop_front();
    }
    /* Regardless, construct our brand new uops. */
    return new (space) uop_t[num_uops];
}

void return_uop_array(struct uop_t* p, const size_t num_uops) {
    assert(num_uops > 0 && num_uops <= MAX_NUM_UOPS);
    /* Make sure we destruct all uops. */
    for (size_t i = 0; i < num_uops; i++)
        p[i].~uop_t();

    /* Add uop array to appropriate free list. */
    auto& free_list = uop_free_lists[num_uops];
    free_list.push_front(p);
}

static list<xed_reg_enum_t> get_registers_read(const struct Mop_t* Mop);
static list<xed_reg_enum_t> get_registers_written(const struct Mop_t* Mop);
static list<xed_reg_enum_t>
get_memory_operand_registers_read(const struct Mop_t* Mop, size_t mem_op);

/* Helper to fill out flags and registers for a load uop. */
static void
fill_out_load_uop(const struct Mop_t* Mop, struct uop_t& load_uop, size_t mem_op_index = 0) {
    load_uop.decode.is_load = true;
    load_uop.decode.FU_class = FU_LD;
    load_uop.oracle.mem_op_index = mem_op_index;

    auto mem_regs = get_memory_operand_registers_read(Mop, mem_op_index);
    size_t reg_ind = 0;
    for (auto it = mem_regs.begin(); it != mem_regs.end(); it++, reg_ind++)
        load_uop.decode.idep_name[reg_ind] = *it;
}

/* Helper to fill out flags and registers for a sta uop. */
static void
fill_out_sta_uop(const struct Mop_t* Mop, struct uop_t& sta_uop, size_t mem_op_index = 0) {
    sta_uop.decode.is_sta = true;
    sta_uop.decode.FU_class = FU_STA;
    sta_uop.oracle.mem_op_index = mem_op_index;

    auto mem_regs = get_memory_operand_registers_read(Mop, mem_op_index);
    size_t reg_ind = 0;
    for (auto it = mem_regs.begin(); it != mem_regs.end(); it++, reg_ind++)
        sta_uop.decode.idep_name[reg_ind] = *it;
}

/* Helper to fill out flags and registers for a std uop. */
static void
fill_out_std_uop(const struct Mop_t* Mop, struct uop_t& std_uop, const xed_reg_enum_t data_reg, size_t mem_op_index = 0) {
    std_uop.decode.is_std = true;
    std_uop.decode.FU_class = FU_STD;
    std_uop.oracle.mem_op_index = mem_op_index;

    std_uop.decode.idep_name[0] = data_reg;
}

/* Create a 1-uop mapping for this Mop, keeping:
 * - register input / output same as the Mop
 * - flags (load/store/jump) same as the Mop
 */
static void fallback(struct Mop_t* Mop) {
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
    Mop->allocate_uops();

    /* Fill out flags for main uop. */
    struct uop_t& main_uop = Mop->uop[op_index];
    main_uop.decode.is_ctrl = Mop->decode.is_ctrl;
    main_uop.decode.is_nop = is_nop(Mop);
    main_uop.decode.is_fence = is_fence(Mop);
    main_uop.decode.is_fpop = is_fp(Mop);

    /* Check functional unit tables for main uop. */
    main_uop.decode.FU_class = get_uop_fu(main_uop);

    /* Fill out register read dependences from XED.
     * All of them go to the main uop. */
    auto ideps = get_registers_read(Mop);
    assert(ideps.size() <= MAX_IDEPS);
    /* Reserve idep 0 for the load->op dependence through a temp register. */
    int idep_ind = Mop_has_load ? 1 : 0;
    for (auto it = ideps.begin(); it != ideps.end(); it++, idep_ind++) {
        main_uop.decode.idep_name[idep_ind] = *it;
    }

    /* Similarly for register write dependences. */
    auto odeps = get_registers_written(Mop);
    assert(odeps.size() <= MAX_ODEPS);
    /* Reserve odep 0 for the op->store dependence through a temp register. */
    int odep_ind = Mop_has_store ? 1 : 0;
    for (auto it = odeps.begin(); it != odeps.end(); it++, odep_ind++) {
        main_uop.decode.odep_name[odep_ind] = *it;
    }

    /* First uop is a load, and generates an address to a temp register. */
    if (Mop_has_load) {
        fill_out_load_uop(Mop, Mop->uop[0]);
        Mop->uop[0].decode.odep_name[0] = XED_REG_TMP0;

        assert(main_uop.decode.idep_name[0] == XED_REG_INVALID);
        main_uop.decode.idep_name[0] = XED_REG_TMP0;
    }

    /* Last two uops are STA and STD. */
    if (Mop_has_store) {
        auto std_temp_reg = Mop_has_load ? XED_REG_TMP1 : XED_REG_TMP0;
        assert(main_uop.decode.odep_name[0] == XED_REG_INVALID);
        main_uop.decode.odep_name[0] = std_temp_reg;

        fill_out_sta_uop(Mop, Mop->uop[op_index + 1]);
        fill_out_std_uop(Mop, Mop->uop[op_index + 2], std_temp_reg);
    }
}

static bool check_load(const struct Mop_t* Mop) {
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
    case XED_IFORM_LODSB:
    case XED_IFORM_LODSW:
    case XED_IFORM_LODSD:
    case XED_IFORM_LODSQ:
        return true;
    default:
        return false;
    }
    return false;
}

static bool check_store(const struct Mop_t* Mop) {
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
    case XED_IFORM_STOSB:
    case XED_IFORM_STOSD:
    case XED_IFORM_STOSQ:
    case XED_IFORM_STOSW:
        return true;
    default:
        return false;
    }
    return false;
}

static bool check_call(const struct Mop_t* Mop) {
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

static bool check_ret(const struct Mop_t* Mop) {
    auto iclass = xed_decoded_inst_get_iclass(&Mop->decode.inst);
    switch (iclass) {
    case XED_ICLASS_RET_NEAR:
        return true;
    default:
        return false;
    }
    return false;
}

static bool check_push(const struct Mop_t* Mop) {
    auto iclass = xed_decoded_inst_get_iclass(&Mop->decode.inst);
    switch (iclass) {
    case XED_ICLASS_PUSH:
    // case XED_ICLASS_PUSHA: -- XXX: these are crazy microcoded, later
    // case XED_ICLASS_PUSHAD:
    case XED_ICLASS_PUSHF:
    case XED_ICLASS_PUSHFD:
    case XED_ICLASS_PUSHFQ:
        return true;
    default:
        return false;
    }

    return false;
}

static bool check_pop(const struct Mop_t* Mop) {
    auto iclass = xed_decoded_inst_get_iclass(&Mop->decode.inst);
    switch (iclass) {
    case XED_ICLASS_POP:
    // case XED_ICLASS_POPA:
    // case XED_ICLASS_POPAD:
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
static bool check_mul(const struct Mop_t* Mop) {
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
    /* XED_IFORM_IMUL_MEMb actually just writes AX. Sigh. */
    case XED_IFORM_IMUL_MEMv:
        return true;
    default:
        return false;
    }
    return false;
}

/* Check for the DIV and IDIV variants that have two
 * output registers (rAX and rDX).
 * The other variants can be handled by the fallback path. */
static bool check_div(const struct Mop_t* Mop) {
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

static bool check_cpuid(const struct Mop_t* Mop) {
    auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
    switch (iform) {
    case XED_IFORM_CPUID:
        return true;
    default:
        return false;
    }

    return false;
}

static bool check_lea(const struct Mop_t* Mop) {
    auto iclass = xed_decoded_inst_get_iclass(&Mop->decode.inst);
    switch (iclass) {
    case XED_ICLASS_LEA:
        return true;
    default:
        return false;
    }

    return false;
}

/* MOVS is a direct load-store transfer with two different mem operands. */
static bool check_movs(const struct Mop_t* Mop) {
    auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
    switch (iform) {
    case XED_IFORM_MOVSB:
    case XED_IFORM_MOVSW:
    case XED_IFORM_MOVSD:
    case XED_IFORM_MOVSQ:
        return true;
    default:
        return false;
    }

    return false;

}

static bool check_cmps(const struct Mop_t* Mop) {
    auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
    switch (iform) {
    case XED_IFORM_CMPSB:
    case XED_IFORM_CMPSW:
    case XED_IFORM_CMPSD:
    case XED_IFORM_CMPSQ:
        return true;
    default:
        return false;
    }

    return false;

}

/* Check special-casing Mop->uop tables. Returns
 * true and modifies Mop if it has found a cracking.
 */
static bool check_tables(struct Mop_t* Mop) {
    if (check_load(Mop)) {
        Mop->decode.flow_length = 1;
        Mop->allocate_uops();

        /* Set flags and implicit register dependences. */
        fill_out_load_uop(Mop, Mop->uop[0]);

        /* Set output register. */
        auto regs_written = get_registers_written(Mop);
        assert(regs_written.size() == 1);
        Mop->uop[0].decode.odep_name[0] = regs_written.front();
        return true;
    }

    if (check_store(Mop)) {
        Mop->decode.flow_length = 2;
        Mop->allocate_uops();

        fill_out_sta_uop(Mop, Mop->uop[0]);

        /* Set std input register, if any. */
        xed_reg_enum_t std_reg = XED_REG_INVALID;
        auto regs_read = get_registers_read(Mop);
        if (regs_read.size() > 0) {
            //XXX: STOS is predicated on flags, ignore for now.
            //assert(regs_read.size() == 1);
            std_reg = regs_read.front();
        }
        fill_out_std_uop(Mop, Mop->uop[1], std_reg);

        return true;
    }

    if (is_nop(Mop)) {
        Mop->decode.flow_length = 1;
        Mop->allocate_uops();

        Mop->uop[0].decode.is_nop = true;
        return true;
    }

    if (check_call(Mop)) {
        Mop->decode.flow_length = 4;
        size_t jmp_ind = 3;
        size_t store_memory_op_index = 0;
        auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
        /* Indirect calls to memory have an additional load uop and an additional
         * memory operand. Ugly. */
        if (iform == XED_IFORM_CALL_NEAR_MEMv) {
            Mop->decode.flow_length++;
            jmp_ind = 4;
            store_memory_op_index = 1;
        }
        Mop->allocate_uops();

        /* uop01: sta std EIP -> [ESP] */
        fill_out_sta_uop(Mop, Mop->uop[0], store_memory_op_index);
        fill_out_std_uop(Mop, Mop->uop[1], XED_REG_INVALID, store_memory_op_index);
        /* std is technically dependant on EIP. But in our model each instruction
         * carries its own IP. */

        /* uop2: Subtract from ESP */
        Mop->uop[2].decode.FU_class = FU_IEU;
        Mop->uop[2].decode.idep_name[0] = XED_REG_ESP;
        Mop->uop[2].decode.odep_name[0] = XED_REG_ESP;

        /* uop(last): jump to target */
        Mop->uop[jmp_ind].decode.FU_class = FU_JEU;
        Mop->uop[jmp_ind].decode.is_ctrl = true;

        /* add target dependences in the indirect case */
        if (iform == XED_IFORM_CALL_NEAR_GPRv) {
            auto regs_read = get_registers_read(Mop);
            assert(regs_read.size() == 1);
            Mop->uop[jmp_ind].decode.idep_name[0] = regs_read.front();
        } else if (iform == XED_IFORM_CALL_NEAR_MEMv) {
            fill_out_load_uop(Mop, Mop->uop[3]);
            Mop->uop[3].decode.odep_name[0] = XED_REG_TMP0;

            Mop->uop[jmp_ind].decode.idep_name[0] = XED_REG_TMP0;
        }
        return true;
    }

    if (check_ret(Mop)) {
        Mop->decode.flow_length = 3;
        Mop->allocate_uops();

        /* uop0: Load jump destination */
        fill_out_load_uop(Mop, Mop->uop[0]);
        Mop->uop[0].decode.odep_name[0] = XED_REG_TMP0;

        /* uop1: Add to ESP */
        Mop->uop[1].decode.FU_class = FU_IEU;
        Mop->uop[1].decode.idep_name[0] = XED_REG_ESP;
        Mop->uop[1].decode.odep_name[0] = XED_REG_ESP;

        /* uop2: jump to target */
        Mop->uop[2].decode.FU_class = FU_JEU;
        Mop->uop[2].decode.is_ctrl = true;
        Mop->uop[2].decode.idep_name[0] = XED_REG_TMP0;

        return true;
    }

    if (check_push(Mop)) {
        Mop->decode.flow_length = 3;
        bool has_load = is_load(Mop);
        size_t store_memory_op_index = 0;
        if (has_load) {
            Mop->decode.flow_length++;
            store_memory_op_index = 1;
        }
        Mop->allocate_uops();

        if (has_load) {
            fill_out_load_uop(Mop, Mop->uop[0]);
            Mop->uop[0].decode.odep_name[0] = XED_REG_TMP0;
        }

        size_t flow_start = has_load ? 1 : 0;

        /* uop01: sta std for stack push */
        fill_out_sta_uop(Mop, Mop->uop[flow_start], store_memory_op_index);
        auto data_reg = XED_REG_INVALID;
        if (has_load) {
            data_reg = XED_REG_TMP0;
        } else {
            auto regs_read = get_registers_read(Mop);
            if (regs_read.size())
                data_reg = regs_read.front();
            /* else, this is the immediate case */
        }
        fill_out_std_uop(Mop, Mop->uop[flow_start + 1], data_reg, store_memory_op_index);

        /* uop2: Subtract from ESP */
        Mop->uop[flow_start + 2].decode.FU_class = FU_IEU;
        Mop->uop[flow_start + 2].decode.idep_name[0] = XED_REG_ESP;
        Mop->uop[flow_start + 2].decode.odep_name[0] = XED_REG_ESP;
        return true;
    }

    if (check_pop(Mop)) {
        Mop->decode.flow_length = 2;
        bool has_store = is_store(Mop);
        size_t read_mem_operand_ind = 0;
        if (has_store) {
            Mop->decode.flow_length += 2;
            read_mem_operand_ind = 1;
        }
        Mop->allocate_uops();

        fill_out_load_uop(Mop, Mop->uop[0], read_mem_operand_ind);
        if (has_store) {
            Mop->uop[0].decode.odep_name[0] = XED_REG_TMP0;
        } else {
            Mop->uop[0].decode.odep_name[0] = get_registers_written(Mop).front();
        }

        /* uop1: Add to ESP */
        Mop->uop[1].decode.FU_class = FU_IEU;
        Mop->uop[1].decode.idep_name[0] = XED_REG_ESP;
        Mop->uop[1].decode.odep_name[0] = XED_REG_ESP;

        if (has_store) {
            fill_out_sta_uop(Mop, Mop->uop[2], 0);
            fill_out_std_uop(Mop, Mop->uop[3], XED_REG_TMP0, 0);
        }
        return true;
    }

    /* Some (I)MUL and (I)DIV variants write to two output registers. */
    if (check_mul(Mop) || check_div(Mop)) {
        Mop->decode.flow_length = 3;
        Mop->allocate_uops();

        if (is_load(Mop)) {
            fill_out_load_uop(Mop, Mop->uop[0]);
        } else {
            Mop->uop[0].decode.FU_class = FU_IEU;
            /* regs_read[0] is the explicit register */
            Mop->uop[0].decode.idep_name[0] = get_registers_read(Mop).front();
        }
        Mop->uop[0].decode.odep_name[0] = XED_REG_TMP0;

        Mop->uop[1].decode.FU_class = get_uop_fu(Mop->uop[1]);
        Mop->uop[1].decode.idep_name[0] = XED_REG_EAX;
        Mop->uop[1].decode.idep_name[1] = XED_REG_TMP0;
        Mop->uop[1].decode.odep_name[0] = XED_REG_EAX;

        Mop->uop[2].decode.FU_class = get_uop_fu(Mop->uop[2]);
        Mop->uop[2].decode.idep_name[0] = XED_REG_EAX;
        Mop->uop[2].decode.idep_name[1] = XED_REG_TMP0;
        Mop->uop[2].decode.odep_name[0] = XED_REG_EDX;
        return true;
    }

    if (check_lea(Mop)) {
        Mop->decode.flow_length = 1;
        Mop->allocate_uops();

        /* Treat as load to generate correct dependences, then overwrite flags and FUs. */
        fill_out_load_uop(Mop, Mop->uop[0]);
        Mop->uop[0].decode.is_load = false;
        Mop->uop[0].decode.is_agen = true;
        Mop->uop[0].decode.FU_class = FU_AGEN;

        /* Set output register. */
        auto regs_written = get_registers_written(Mop);
        assert(regs_written.size() == 1);
        Mop->uop[0].decode.odep_name[0] = regs_written.front();
        return true;
    }

    /* MOVS is a direct load-store transfer. Two different mem operands.
     * XXX: Memory operands in XED appear flipped: load is 1, store is 0. */
    if (check_movs(Mop)) {
        Mop->decode.flow_length = 3;
        Mop->allocate_uops();

        fill_out_load_uop(Mop, Mop->uop[0], 1);
        Mop->uop[0].decode.odep_name[0] = XED_REG_TMP0;
        fill_out_sta_uop(Mop, Mop->uop[1], 0);
        fill_out_std_uop(Mop, Mop->uop[2], XED_REG_TMP0, 0);
        return true;
    }

    /* CMPS is two loads with a compare. */
    if (check_cmps(Mop)) {
        Mop->decode.flow_length = 3;
        Mop->allocate_uops();

        fill_out_load_uop(Mop, Mop->uop[0], 0);
        Mop->uop[0].decode.odep_name[0] = XED_REG_TMP0;
        fill_out_load_uop(Mop, Mop->uop[1], 1);
        Mop->uop[1].decode.odep_name[0] = XED_REG_TMP1;

        Mop->uop[2].decode.FU_class = FU_IEU;
        Mop->uop[2].decode.idep_name[0] = XED_REG_TMP0;
        Mop->uop[2].decode.idep_name[1] = XED_REG_TMP1;
        Mop->uop[2].decode.odep_name[0] = XED_REG_EFLAGS;
        return true;
    }

    /* CPUID writes to four output registers. */
    if (check_cpuid(Mop)) {
        Mop->decode.flow_length = 4;
        Mop->allocate_uops();
        /* XXX: It's also a trap (serializing instruction).
           One of our invariants is that traps don't go to FUs.
           But in that case, we won't handle register dependences properly.
           So, just don't specify the output regs. The trap will drain the
           pipeline anyway.
         */
        return true;
    }

    return false;
}

void crack(struct Mop_t* Mop) {
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
static list<xed_reg_enum_t> get_registers_read(const struct Mop_t* Mop) {
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

            /* XED lists many instructions (unnecessarily?) dependant on the IP.
             * Ignore that, in our model every instrution carries its IP. */
            if (largest_reg == XED_REG_EIP)
                continue;

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
static list<xed_reg_enum_t> get_registers_written(const struct Mop_t* Mop) {
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

/* Get an instruction's implicitly read registers
 * (through memory operand address generation).
 */
static list<xed_reg_enum_t>
get_memory_operand_registers_read(const struct Mop_t* Mop, size_t mem_op) {
    list<xed_reg_enum_t> res;
#ifndef NDEBUG
    size_t num_mem = xed_decoded_inst_number_of_memory_operands(&Mop->decode.inst);
    assert(mem_op < num_mem);
#endif

    /* Add mem op base register, if any */
    xed_reg_enum_t base_reg = xed_decoded_inst_get_base_reg(&Mop->decode.inst, mem_op);
    if (base_reg != XED_REG_INVALID)
        res.push_back(base_reg);
#ifdef DEBUG_CRACKER
    std::cerr << xed_reg_enum_t2str(base_reg) << std::endl;
#endif

    /* Add mem op index register, if any */
    xed_reg_enum_t index_reg = xed_decoded_inst_get_index_reg(&Mop->decode.inst, mem_op);
#ifdef DEBUG_CRACKER
    std::cerr << xed_reg_enum_t2str(index_reg) << std::endl;
#endif
    if (index_reg != XED_REG_INVALID)
        res.push_back(index_reg);

    /* XXX: should we do something about segements? They barely ever get written, so it probably
     * doesn't matter. */
    /* XXX: should we do something special about RIP-relative addressing in x64? */

    return res;
}
}  // xiosim::x86
}  // xiosim
