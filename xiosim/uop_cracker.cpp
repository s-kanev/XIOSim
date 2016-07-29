#include <cstddef>
#include <iostream>
#include <list>

#include "decode.h"
#include "misc.h"
#include "regs.h"
#include "uop_cracker.h"
#include "zesto-structs.h"

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
 * TODO(skanev): move this pool to a separate file.
 */
class uop_pool_t {
  public:
    struct uop_t* get_uop_array(const size_t num_uops) {
        xiosim_assert(num_uops > 0 && num_uops <= MAX_NUM_UOPS);
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
        xiosim_assert(num_uops > 0 && num_uops <= MAX_NUM_UOPS);
        /* Make sure we destruct all uops. */
        for (size_t i = 0; i < num_uops; i++)
            p[i].~uop_t();

        /* Add uop array to appropriate free list. */
        auto& free_list = uop_free_lists[num_uops];
        free_list.push_front(p);
    }

    ~uop_pool_t() {
        for (size_t i = 0; i <= MAX_NUM_UOPS; i++) {
            for (uop_t* ptr : uop_free_lists[i]) {
                free(ptr);
            }
        }
    }

  protected:
    std::list<uop_t*> uop_free_lists[MAX_NUM_UOPS + 1];
};
static thread_local uop_pool_t uop_pool;
struct uop_t* get_uop_array(const size_t num_uops) {
    return uop_pool.get_uop_array(num_uops);
}
void return_uop_array(uop_t* p, const size_t num_uops) { uop_pool.return_uop_array(p, num_uops); }

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
static void fill_out_std_uop(const struct Mop_t* Mop,
                             struct uop_t& std_uop,
                             const xed_reg_enum_t data_reg,
                             size_t mem_op_index = 0) {
    std_uop.decode.is_std = true;
    std_uop.decode.FU_class = FU_STD;
    std_uop.decode.fusable.STA_STD = true;
    std_uop.oracle.mem_op_index = mem_op_index;

    std_uop.decode.idep_name[0] = data_reg;
}

static void fill_out_cmov_uops(const struct Mop_t* Mop,
                               struct uop_t& cmov_uop1,
                               struct uop_t& cmov_uop2,
                               const xed_reg_enum_t src_op,
                               const xed_reg_enum_t dst_op,
                               const xed_reg_enum_t tmp_reg) {

    cmov_uop1.decode.FU_class = FU_IEU;
    cmov_uop1.decode.idep_name[0] = largest_reg(XED_REG_EFLAGS);
    cmov_uop1.decode.idep_name[1] = src_op;
    cmov_uop1.decode.odep_name[0] = tmp_reg;

    cmov_uop2.decode.FU_class = FU_IEU;
    cmov_uop2.decode.idep_name[0] = tmp_reg;
    cmov_uop2.decode.odep_name[0] = dst_op;
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

    auto iclass = xed_decoded_inst_get_iclass(&Mop->decode.inst);
    /* Fill out flags for main uop. */
    struct uop_t& main_uop = Mop->uop[op_index];
    main_uop.decode.is_ctrl = Mop->decode.is_ctrl;
    main_uop.decode.is_nop = is_nop(Mop);
    // XXX: handle fences outside of fallback().
    main_uop.decode.is_mfence = (iclass == XED_ICLASS_MFENCE);
    main_uop.decode.is_lfence = (iclass == XED_ICLASS_LFENCE) || main_uop.decode.is_mfence;
    main_uop.decode.is_sfence = (iclass == XED_ICLASS_SFENCE) || main_uop.decode.is_mfence;
    main_uop.decode.is_fpop = is_fp(Mop);

    /* Flag for load-op uop fusion. */
    if (Mop_has_load && !main_uop.decode.is_ctrl) {
        /* We don't fuse indirect jmps. Mostly for the Atom pipes which have
         * the JEU and load unit on separate ports. */

        if (main_uop.decode.is_fpop)
            main_uop.decode.fusable.FP_LOAD_OP = true;
        else
            main_uop.decode.fusable.LOAD_OP = true;
    }

    /* Check functional unit tables for main uop. */
    main_uop.decode.FU_class = get_uop_fu(main_uop);

    /* Fill out register read dependences from XED.
     * All of them go to the main uop. */
    auto ideps = get_registers_read(Mop);
    xiosim_assert(ideps.size() <= MAX_IDEPS);
    /* Reserve idep 0 for the load->op dependence through a temp register. */
    int idep_ind = Mop_has_load ? 1 : 0;
    for (auto it = ideps.begin(); it != ideps.end(); it++, idep_ind++) {
        main_uop.decode.idep_name[idep_ind] = *it;
    }

    /* Similarly for register write dependences. */
    auto odeps = get_registers_written(Mop);
    xiosim_assert(odeps.size() <= MAX_ODEPS);
    /* Reserve odep 0 for the op->store dependence through a temp register. */
    int odep_ind = Mop_has_store ? 1 : 0;
    for (auto it = odeps.begin(); it != odeps.end(); it++, odep_ind++) {
        main_uop.decode.odep_name[odep_ind] = *it;
    }

    /* First uop is a load, and generates an address to a temp register. */
    if (Mop_has_load) {
        fill_out_load_uop(Mop, Mop->uop[0]);
        Mop->uop[0].decode.odep_name[0] = XED_REG_TMP0;

        xiosim_assert(main_uop.decode.idep_name[0] == XED_REG_INVALID);
        main_uop.decode.idep_name[0] = XED_REG_TMP0;
    }

    /* Last two uops are STA and STD. */
    if (Mop_has_store) {
        auto std_temp_reg = Mop_has_load ? XED_REG_TMP1 : XED_REG_TMP0;
        xiosim_assert(main_uop.decode.odep_name[0] == XED_REG_INVALID);
        main_uop.decode.odep_name[0] = std_temp_reg;

        fill_out_sta_uop(Mop, Mop->uop[op_index + 1]);
        fill_out_std_uop(Mop, Mop->uop[op_index + 2], std_temp_reg);
    }

    /* Flag for Macro-op execution -- load-op-store fusion. */
    if (Mop_has_load && Mop_has_store) {
        main_uop.decode.fusable.LOAD_OP_ST = true;
        Mop->uop[op_index + 1].decode.fusable.LOAD_OP_ST = true;
        Mop->uop[op_index + 2].decode.fusable.LOAD_OP_ST = true;
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
    case XED_IFORM_MOVSD_XMM_XMMdq_MEMsd:
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
    /* Treat SW prefetches as loads. We'll set a special flag for them (no odep),
     * so the rest of the pipeline can handle them differently. */
    case XED_IFORM_PREFETCHNTA_MEMmprefetch:
    case XED_IFORM_PREFETCHT0_MEMmprefetch:
    case XED_IFORM_PREFETCHT1_MEMmprefetch:
    case XED_IFORM_PREFETCHT2_MEMmprefetch:
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
    case XED_IFORM_MOVSD_XMM_MEMsd_XMMsd:
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

static bool check_rdtsc(const struct Mop_t* Mop) {
    auto iclass = xed_decoded_inst_get_iclass(&Mop->decode.inst);
    switch (iclass) {
    case XED_ICLASS_RDTSC:
    case XED_ICLASS_RDTSCP:
        return true;
    default:
        return false;
    }

    return false;
}

static bool check_sincos(const struct Mop_t* Mop) {
    auto iclass = xed_decoded_inst_get_iclass(&Mop->decode.inst);
    switch (iclass) {
    case XED_ICLASS_FSINCOS:
        return true;
    default:
        return false;
    }

    return false;
}

static bool check_cmovcc(const struct Mop_t* Mop) {
    auto icat = xed_decoded_inst_get_category(&Mop->decode.inst);
    switch (icat) {
    case XED_CATEGORY_CMOV:
        return true;
    default:
        return false;
    }

    return false;
}

static bool check_cmpxchg(const struct Mop_t* Mop) {
    auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
    switch (iform) {
    case XED_IFORM_CMPXCHG_GPRv_GPRv:
    case XED_IFORM_CMPXCHG_GPR8_GPR8:
    case XED_IFORM_CMPXCHG_MEMv_GPRv:
    case XED_IFORM_CMPXCHG_MEMb_GPR8:
#if 0
    /* These have some evil flows with 15/22 uops. Instead of implementing them,
     * I'll run away and scream for a while. */
    case XED_IFORM_CMPXCHG16B_MEMdq:
    case XED_IFORM_CMPXCHG8B_MEMq:
#endif
        return true;
    default:
        return false;
    }

    return false;
}

static bool check_xchg(const struct Mop_t* Mop) {
    auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
    switch (iform) {
    case XED_IFORM_XCHG_GPRv_GPRv:
    case XED_IFORM_XCHG_GPRv_OrAX:
    case XED_IFORM_XCHG_GPR8_GPR8:
    case XED_IFORM_XCHG_MEMv_GPRv:
    case XED_IFORM_XCHG_MEMb_GPR8:
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
        xiosim_assert(regs_written.size() <= 1);
        if (regs_written.size())
            Mop->uop[0].decode.odep_name[0] = regs_written.front();
        else
            Mop->uop[0].decode.is_pf = true;
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
            // XXX: STOS is predicated on flags, ignore for now.
            // xiosim_assert(regs_read.size() == 1);
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
        Mop->decode.flow_length = 3;
        size_t jmp_ind = 2;
        size_t store_memory_op_index = 0;
        auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
        /* Indirect calls to memory have an additional load uop and an additional
         * memory operand. Ugly. */
        if (iform == XED_IFORM_CALL_NEAR_MEMv) {
            Mop->decode.flow_length++;
            jmp_ind++;
            store_memory_op_index = 1;
        }
        Mop->allocate_uops();

        /* uop01: sta std EIP -> [ESP] */
        fill_out_sta_uop(Mop, Mop->uop[0], store_memory_op_index);
        fill_out_std_uop(Mop, Mop->uop[1], XED_REG_INVALID, store_memory_op_index);
        /* std is technically dependant on EIP. But in our model each instruction
         * carries its own IP. */

        /* Subtract from ESP is handled by the stack engine in most cases. */

        /* uop(last): jump to target */
        Mop->uop[jmp_ind].decode.FU_class = FU_JEU;
        Mop->uop[jmp_ind].decode.is_ctrl = true;

        /* add target dependences in the indirect case */
        if (iform == XED_IFORM_CALL_NEAR_GPRv) {
            auto regs_read = get_registers_read(Mop);
            xiosim_assert(regs_read.size() == 1);
            Mop->uop[jmp_ind].decode.idep_name[0] = regs_read.front();
        } else if (iform == XED_IFORM_CALL_NEAR_MEMv) {
            fill_out_load_uop(Mop, Mop->uop[jmp_ind - 1]);
            Mop->uop[jmp_ind - 1].decode.odep_name[0] = XED_REG_TMP0;

            Mop->uop[jmp_ind].decode.idep_name[0] = XED_REG_TMP0;
        }
        return true;
    }

    if (check_ret(Mop)) {
        Mop->decode.flow_length = 2;
        Mop->allocate_uops();

        /* uop0: Load jump destination */
        fill_out_load_uop(Mop, Mop->uop[0]);
        Mop->uop[0].decode.odep_name[0] = XED_REG_TMP0;

        /* Add to ESP is handled by the stack engine in most cases. */

        /* uop1: jump to target */
        Mop->uop[1].decode.FU_class = FU_JEU;
        Mop->uop[1].decode.is_ctrl = true;
        Mop->uop[1].decode.idep_name[0] = XED_REG_TMP0;

        return true;
    }

    if (check_push(Mop)) {
        Mop->decode.flow_length = 2;
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
        /* Subtract from ESP is handled by the stack engine in most cases. */
        return true;
    }

    if (check_pop(Mop)) {
        Mop->decode.flow_length = 1;
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

        /* Add to ESP is handled by the stack engine in most cases. */

        if (has_store) {
            fill_out_sta_uop(Mop, Mop->uop[1], 0);
            fill_out_std_uop(Mop, Mop->uop[2], XED_REG_TMP0, 0);
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
        Mop->uop[1].decode.idep_name[0] = largest_reg(XED_REG_EAX);
        Mop->uop[1].decode.idep_name[1] = XED_REG_TMP0;
        Mop->uop[1].decode.odep_name[0] = largest_reg(XED_REG_EAX);

        Mop->uop[2].decode.FU_class = get_uop_fu(Mop->uop[2]);
        Mop->uop[2].decode.idep_name[0] = largest_reg(XED_REG_EAX);
        Mop->uop[2].decode.idep_name[1] = XED_REG_TMP0;
        Mop->uop[2].decode.odep_name[0] = largest_reg(XED_REG_EDX);
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
        xiosim_assert(regs_written.size() == 1);
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
        Mop->uop[2].decode.odep_name[0] = largest_reg(XED_REG_EFLAGS);
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

    /* RDTSC and RDTSCP write to 2 and 3 output registers. */
    if (check_rdtsc(Mop)) {
        auto regs_written = get_registers_written(Mop);
        size_t n_regs = regs_written.size();

        Mop->decode.flow_length = n_regs;
        Mop->allocate_uops();

        size_t i = 0;
        for (auto reg : regs_written) {
            Mop->uop[i].decode.FU_class = FU_IEU;
            Mop->uop[i].decode.odep_name[0] = reg;
            i++;
        }
        return true;
    }

    /* FSINCOS writes to 2 FP registers. */
    if (check_sincos(Mop)) {
        Mop->decode.flow_length = 2;
        Mop->allocate_uops();

        Mop->uop[0].decode.FU_class = FU_FCPLX;
        Mop->uop[0].decode.idep_name[0] = XED_REG_ST0;
        Mop->uop[0].decode.odep_name[0] = XED_REG_ST1;

        Mop->uop[1].decode.FU_class = FU_FCPLX;
        Mop->uop[1].decode.idep_name[0] = XED_REG_ST0;
        Mop->uop[1].decode.odep_name[0] = XED_REG_ST0;
        Mop->uop[1].decode.odep_name[1] = XED_REG_X87STATUS;
        return true;
    }

    /* CMOVcc has 2 dependent uops.
     * XXX: CMOVBE/NBE/A/NA apparently have 3, which we'll ignore.
     * XXX: on BDW and newer, it's one fewer, which we'll also ignore. */
    if (check_cmovcc(Mop)) {
        Mop->decode.flow_length = 2;
        Mop->allocate_uops();

        fill_out_cmov_uops(Mop, Mop->uop[0], Mop->uop[1], get_registers_read(Mop).front(),
                           get_registers_written(Mop).front(), XED_REG_TMP0);
        return true;
    }

    /* CMPXCHG is ... interesting.
     * Agner Fog (and HSW measurements) suggests 6 uops (5 after fusion) for the mem-reg
     * iform with 8 throughput (we count STA/STD separately, so that would be 7 of our uops).
     * In the natural breakdown with 2-operand uops,
     * (LD->CMP->3x CMOVs (1 for rAX, 2 with inverted conditions for the STD)->STA/STD)
     * 3 of these are CMOV uops though, and they seem to break down to 2 uops each
     * (see above). We'll assume that even though from measurements 2 of these CMOVs
     * seem 1-uop.
     * (XXX: the difference might be between a CMOV with temp reg operands and a CMOV with
     * a physical register operand).
     * The reg-reg form measures 9 cycles and 9 uops on HSW?! No idea why, so I'll just keep
     * our simple model with 3 cycles and 7 uops. Reg-reg should be really rare anyways. */
    if (check_cmpxchg(Mop)) {
        bool has_load = is_load(Mop);
        bool has_lock = Mop->decode.opflags.ATOMIC;

        Mop->decode.flow_length = has_load ? 10 : 7;
        if (has_lock)
            Mop->decode.flow_length += 4;
        size_t start_ind = 0;
        Mop->allocate_uops();

        xed_reg_enum_t dst_operand, src_operand;
        auto regs_read = get_registers_read(Mop);
        if (has_load) {
            start_ind = 1;

            if (has_lock) {
                uop_t& mfence_uop_1 = Mop->uop[0];
                mfence_uop_1.decode.FU_class = FU_LD;
                mfence_uop_1.decode.is_lfence = true;
                mfence_uop_1.decode.is_mfence = true;
                start_ind++;

                uop_t& mfence_uop_2 = Mop->uop[1];
                mfence_uop_2.decode.FU_class = FU_STA;
                mfence_uop_2.decode.is_sfence = true;
                mfence_uop_2.decode.is_mfence = true;
                start_ind++;
            }

            fill_out_load_uop(Mop, Mop->uop[start_ind - 1], 0);
            Mop->uop[start_ind - 1].decode.odep_name[0] = XED_REG_TMP0;
            dst_operand = XED_REG_TMP0;
            src_operand = regs_read.front();
        } else {
            dst_operand = regs_read.front();
            src_operand = *(++regs_read.begin());
        }

        /* compare EAX and load result */
        Mop->uop[start_ind].decode.FU_class = FU_IEU;
        Mop->uop[start_ind].decode.idep_name[0] = largest_reg(XED_REG_EAX);
        Mop->uop[start_ind].decode.idep_name[1] = dst_operand;
        Mop->uop[start_ind].decode.odep_name[0] =
                XED_REG_TMP1;  // same as TMP0 so this can be fused to the load
        Mop->uop[start_ind].decode.odep_name[1] = largest_reg(XED_REG_EFLAGS);
        Mop->uop[start_ind].decode.fusable.LOAD_OP = has_load && !has_lock;

        /* CMOV for mem operand if equal */
        xed_reg_enum_t cmov_dest = has_load ? XED_REG_TMP2 : dst_operand;
        fill_out_cmov_uops(Mop, Mop->uop[start_ind + 1], Mop->uop[start_ind + 2], src_operand, cmov_dest, XED_REG_TMP8);

        /* CMOV for mem operand if not equal */
        fill_out_cmov_uops(Mop, Mop->uop[start_ind + 3], Mop->uop[start_ind + 4], XED_REG_TMP1, cmov_dest, XED_REG_TMP7);

        /* CMOV for EAX */
        fill_out_cmov_uops(Mop, Mop->uop[start_ind + 5], Mop->uop[start_ind + 6], XED_REG_TMP1, largest_reg(XED_REG_EAX), XED_REG_TMP6);

        /* Store happens regardless of the comparison result.
         * Store data is different though. */
        if (has_load) {
            fill_out_sta_uop(Mop, Mop->uop[start_ind + 7], 0);
            fill_out_std_uop(Mop, Mop->uop[start_ind + 8], XED_REG_TMP2);

            if (has_lock) {
                uop_t& mfence_uop_1 = Mop->uop[start_ind + 9];
                mfence_uop_1.decode.FU_class = FU_LD;
                mfence_uop_1.decode.is_lfence = true;
                /* FIXME(skanev): this does need to be an mfence. Otherwise we can get a
                 * ST->LD forwarding between this Mops store and a following load.
                 * Our current implementation of fences doesn't allow two mfences in one Mop
                 * because of atomic commit. */
                //mfence_uop_1.decode.is_mfence = true;

                uop_t& mfence_uop_2 = Mop->uop[start_ind + 10];
                mfence_uop_2.decode.FU_class = FU_STA;
                mfence_uop_2.decode.is_sfence = true;
                mfence_uop_2.decode.is_mfence = true;
            }

        }

        return true;
    }

    if (check_xchg(Mop)) {
        bool has_load = is_load(Mop);
        bool has_lock = Mop->decode.opflags.ATOMIC;

        Mop->decode.flow_length = has_load ? 4 : 3;
        if (has_lock)
            Mop->decode.flow_length += 4;
        size_t curr_ind = 0;
        Mop->allocate_uops();

        if (has_lock) {
            uop_t& mfence_uop_1 = Mop->uop[0];
            mfence_uop_1.decode.FU_class = FU_LD;
            mfence_uop_1.decode.is_lfence = true;
            mfence_uop_1.decode.is_mfence = true;

            uop_t& mfence_uop_2 = Mop->uop[1];
            mfence_uop_2.decode.FU_class = FU_STA;
            mfence_uop_2.decode.is_sfence = true;
            mfence_uop_2.decode.is_mfence = true;
            curr_ind += 2;
        }

        auto regs_read = get_registers_read(Mop);
        auto regs_written = get_registers_written(Mop);

        if (has_load) {
            fill_out_load_uop(Mop, Mop->uop[curr_ind], 0);
            Mop->uop[curr_ind].decode.odep_name[0] = XED_REG_TMP0;

            fill_out_sta_uop(Mop, Mop->uop[curr_ind + 1], 0);
            fill_out_std_uop(Mop, Mop->uop[curr_ind + 2], regs_read.front());
            curr_ind += 3;
        } else {
            Mop->uop[curr_ind].decode.FU_class = FU_IEU;
            Mop->uop[curr_ind].decode.idep_name[0] = *(++regs_read.begin());
            Mop->uop[curr_ind].decode.odep_name[0] = XED_REG_TMP0;

            Mop->uop[curr_ind + 1].decode.FU_class = FU_IEU;
            Mop->uop[curr_ind + 1].decode.idep_name[0] = regs_read.front();
            Mop->uop[curr_ind + 1].decode.odep_name[0] = *(++regs_written.begin());
            curr_ind += 2;
        }

        Mop->uop[curr_ind].decode.FU_class = FU_IEU;
        Mop->uop[curr_ind].decode.idep_name[0] = XED_REG_TMP0;
        Mop->uop[curr_ind].decode.odep_name[0] = regs_written.front();
        curr_ind++;

        if (has_lock) {
            uop_t& mfence_uop_1 = Mop->uop[curr_ind];
            mfence_uop_1.decode.FU_class = FU_LD;
            mfence_uop_1.decode.is_lfence = true;
            //FIXME(skanev): same as above.
            //mfence_uop_1.decode.is_mfence = true;

            uop_t& mfence_uop_2 = Mop->uop[curr_ind + 1];
            mfence_uop_2.decode.FU_class = FU_STA;
            mfence_uop_2.decode.is_sfence = true;
            mfence_uop_2.decode.is_mfence = true;
            curr_ind += 2;
        }
        return true;
    }

    return false;
}

/* Check if this Mop is a magic TCMalloc insns that needs a special uop flow.  */
static bool check_magic_insns(struct Mop_t* Mop) {
    if (!Mop->decode.is_magic)
        return false;
    auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
    if (iform != XED_IFORM_BLSR_VGPR64q_VGPR64q && iform != XED_IFORM_BLSR_VGPR32d_VGPR32d)
        return false;

    // BLSR is the SizeClassCacheLookup instruction, which writes to both
    // the destination and source registers in addition to flags.
    // uop 0 reads from src, writes to dest.
    // uop 1 reads from src, writes to src and flags.
    Mop->decode.flow_length = 2;
    Mop->allocate_uops();

    auto ideps = get_registers_read(Mop);
    xiosim_assert(ideps.size() <= MAX_IDEPS);
    int idep_ind = 0;
    for (auto it = ideps.begin(); it != ideps.end(); it++, idep_ind++) {
        Mop->uop[0].decode.idep_name[idep_ind] = *it;
        Mop->uop[1].decode.idep_name[idep_ind] = *it;
    }

    auto odeps = get_registers_written(Mop);
    xiosim_assert(odeps.size() == 2);
    int uop_ind = 0;
    for (auto it = odeps.begin(); it != odeps.end(); it++, uop_ind++) {
        Mop->uop[uop_ind].decode.odep_name[uop_ind] = *it;
    }
    // The second uop writes to its source register.
    Mop->uop[1].decode.odep_name[0] = Mop->uop[1].decode.idep_name[0];

    // First uop performs the size class cache lookups, so it doesn't need
    // to be repeated on the second one, which just updates registers.
    Mop->uop[0].decode.FU_class = FU_SIZE_CLASS;
    Mop->uop[1].decode.FU_class = FU_IEU;
    return true;
}

/* After uop cracking, fix up magic instructions for special behaviors. */
void fixup_magic_insn(struct Mop_t* Mop) {
    if (!Mop->decode.is_magic)
        return;
    auto iclass = xed_decoded_inst_get_iclass(&Mop->decode.inst);
    switch (iclass) {
      case XED_ICLASS_SHLD:
          // SHLD is a size class cache update instruction that does not write
          // to the output operand, only to flags.
          Mop->uop[0].decode.odep_name[0] = largest_reg(XED_REG_EFLAGS);
          Mop->uop[0].decode.odep_name[1] = XED_REG_INVALID;
          Mop->uop[0].decode.FU_class = FU_SIZE_CLASS;
          break;
      case XED_ICLASS_SHRX: {
          // SHRX is a free list cache update instruction. It also does not
          // write to its output operand, but if it is given a memory operand,
          // it will load that address.
          //
          // Instruction is: shrx tmp15, r64, r64/m64. First source is the size
          // class; second is either the pointer or the memory address to
          // dereference. We use tmp15 to represent the size class.
          bool has_load = is_load(Mop);
          size_t uop_ind = has_load ? 1 : 0;
          // Fix up ideps. Set the last idep to TMP15.
          Mop->uop[uop_ind].decode.idep_name[2] = XED_REG_TMP15;
          // Fix up odeps. Set the first odep to TMP15.
          Mop->uop[uop_ind].decode.odep_name[0] = XED_REG_TMP15;
          Mop->uop[uop_ind].decode.odep_name[1] = XED_REG_INVALID;
          Mop->uop[uop_ind].decode.FU_class = FU_SIZE_CLASS;
          if (has_load) {
              // This instruction stores special operands in mem_buffer, so we
              // have to be careful we read memory operands from the right place.
              Mop->uop[0].oracle.mem_op_index = 1;
          }
          break;
      }
      case XED_ICLASS_SHRD:
          Mop->uop[0].decode.FU_class = FU_SIZE_CLASS;
          break;
      default:
          break;
    }
}

void crack(struct Mop_t* Mop) {
#ifdef DEBUG_CRACKER
    auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
    std::cerr << xed_iform_enum_t2str(iform) << std::endl;
#endif

    bool cracked = check_tables(Mop);
    if (cracked)
        return;

    // Is this a magic instruction?
    cracked = check_magic_insns(Mop);
    if (cracked)
        return;

    /* Instead of fully describing Mop->uop tables,
     * we'll try and cover some of the simple common patterns (e.g. LOAD-OP-STORE).
     * We obviously need a mechanism for exceptions for non-standard (e.g. microcoded) ops.
     */
    fallback(Mop);
    fixup_magic_insn(Mop);

    /* there better be at least one uop */
    xiosim_assert(Mop->decode.flow_length);
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
            xed_reg_enum_t encl_reg = largest_reg(reg);

            /* XED lists many instructions (unnecessarily?) dependant on the IP.
             * Ignore that, in our model every instrution carries its IP. */
            if (encl_reg == largest_reg(XED_REG_EIP))
                continue;

            res.push_back(encl_reg);

#ifdef DEBUG_CRACKER
            std::cerr << xed_reg_enum_t2str(reg) << std::endl;
            std::cerr << xed_reg_enum_t2str(encl_reg) << std::endl;
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
            xed_reg_enum_t encl_reg = largest_reg(reg);
            res.push_back(encl_reg);

#ifdef DEBUG_CRACKER
            std::cerr << xed_reg_enum_t2str(reg) << std::endl;
            std::cerr << xed_reg_enum_t2str(encl_reg) << std::endl;
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
    xiosim_assert(mem_op < num_mem);
#endif

    /* Add mem op base register, if any */
    xed_reg_enum_t base_reg = xed_decoded_inst_get_base_reg(&Mop->decode.inst, mem_op);
    if (base_reg != XED_REG_INVALID)
        res.push_back(largest_reg(base_reg));
#ifdef DEBUG_CRACKER
    std::cerr << xed_reg_enum_t2str(base_reg) << std::endl;
#endif

    /* Add mem op index register, if any */
    xed_reg_enum_t index_reg = xed_decoded_inst_get_index_reg(&Mop->decode.inst, mem_op);
#ifdef DEBUG_CRACKER
    std::cerr << xed_reg_enum_t2str(index_reg) << std::endl;
#endif
    if (index_reg != XED_REG_INVALID)
        res.push_back(largest_reg(index_reg));

    /* XXX: should we do something about segements? They barely ever get written, so it probably
     * doesn't matter. */
    /* XXX: should we do something special about RIP-relative addressing in x64? */

    return res;
}

/* Note on the stack engine for push / pop / call / ret flows:
 * It's a small adder in decode that removes the need for an ESP-adjustment uop.
 * There's a wonderful description in http://www.agner.org/optimize/microarchitecture.pdf
 * Our flows don't model the additional fixup uops if we have something like:
 * push eax; add ecx, esp (the add should have one more uop to get the arch ESP)
 * Nor do we model the actual stack delta counter (which seems to be 8 bits)
 * because that would make the cracker stateful.
 * Both these are much rarer cases than a regular push / call, etc.
 */

}  // xiosim::x86
}  // xiosim
