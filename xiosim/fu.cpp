#include <assert.h>

#include "zesto-structs.h"
#include "fu.h"

namespace xiosim {

/* enum fu_class -> description string */
std::string md_fu2name[NUM_FU_CLASSES] = { "", /* NA */
                                           "int-exec-unit", "jump-exec-unit", "int-multiply",
                                           "int-shift",     "FP-add",         "FP-multiply",
                                           "FP-complex",    "FP-divide",      "load-port",
                                           "sta-port",      "std-port",       "agen-unit",
                                           "magic-unit",    "sampling-unit" };

std::string fu_name(enum fu_class fu) {
    assert(fu < NUM_FU_CLASSES);
    return md_fu2name[fu];
}

/* Instruction -> functional unit mappings.
 * This will produce the mapping *for the main uop* of the Mop flow simply
 * based on Mop decoded properties. This covers a large fraction of the common cases,
 * and is mostly useful for x86::fallback(). For more complex, microcoded flows,
 * we have to explicitly specify additional uops' units. */
fu_class get_uop_fu(const struct uop_t& uop) {
    if (uop.decode.is_nop || uop.Mop->decode.is_trap)
        return FU_NA;

    if (uop.decode.is_ctrl)
        return FU_JEU;

    if (uop.decode.is_load)
        return FU_LD;

    if (uop.decode.is_sta)
        return FU_STA;

    if (uop.decode.is_std)
        return FU_STD;

    if (uop.decode.is_agen)
        return FU_AGEN;

    auto icat = xed_decoded_inst_get_category(&uop.Mop->decode.inst);
    if (icat == XED_CATEGORY_SHIFT)
        return FU_SHIFT;

    /* XXX: This switch is starting to get out of control. Consider pre-populating
     * a table that we can index by iclass. Or maybe the compiler is smart enough to
     * do that for us? (Seems no, generates a horrendous if-ladder) */
    auto iclass = xed_decoded_inst_get_iclass(&uop.Mop->decode.inst);
    switch (iclass) {
    /* shifts not covered by XED_CATEGORY_SHIFT */
    case XED_ICLASS_SARX:
    case XED_ICLASS_SHLX:
    case XED_ICLASS_SHRX:
        return FU_SHIFT;
    /* GP integer multiplies */
    case XED_ICLASS_IMUL:
    case XED_ICLASS_MUL:
    case XED_ICLASS_MULX:
        /* TODO: add sse / avx packed muls */
        return FU_IMUL;
    /* GP integer divides */
    case XED_ICLASS_IDIV:
    case XED_ICLASS_DIV:
        /* TODO: add sse / avx packed divs */
        return FU_IDIV;
    /* All flavors of FP adds/subs/cmps */
    /* SSE */
    case XED_ICLASS_ADDPS:
    case XED_ICLASS_ADDSS:
    case XED_ICLASS_CMPPS:
    case XED_ICLASS_CMPSS:
    case XED_ICLASS_MAXPS:
    case XED_ICLASS_MAXSS:
    case XED_ICLASS_MINPS:
    case XED_ICLASS_MINSS:
    case XED_ICLASS_SUBPS:
    case XED_ICLASS_SUBSS:
    /* SSE2 */
    case XED_ICLASS_ADDPD:
    case XED_ICLASS_ADDSD:
    case XED_ICLASS_CMPPD:
    case XED_ICLASS_CMPSD:
    case XED_ICLASS_MAXPD:
    case XED_ICLASS_MAXSD:
    case XED_ICLASS_MINPD:
    case XED_ICLASS_MINSD:
    case XED_ICLASS_SUBPD:
    case XED_ICLASS_SUBSD:
    /* SSE3 */
    case XED_ICLASS_ADDSUBPS:
    case XED_ICLASS_ADDSUBPD:
    /* AVX */
    case XED_ICLASS_VADDPS:
    case XED_ICLASS_VADDSS:
    case XED_ICLASS_VCMPPS:
    case XED_ICLASS_VCMPSS:
    case XED_ICLASS_VMAXPS:
    case XED_ICLASS_VMAXSS:
    case XED_ICLASS_VMINPS:
    case XED_ICLASS_VMINSS:
    case XED_ICLASS_VSUBPS:
    case XED_ICLASS_VSUBSS:
    case XED_ICLASS_VADDPD:
    case XED_ICLASS_VADDSD:
    case XED_ICLASS_VCMPPD:
    case XED_ICLASS_VCMPSD:
    case XED_ICLASS_VMAXPD:
    case XED_ICLASS_VMAXSD:
    case XED_ICLASS_VMINPD:
    case XED_ICLASS_VMINSD:
    case XED_ICLASS_VSUBPD:
    case XED_ICLASS_VSUBSD:
    case XED_ICLASS_VADDSUBPS:
    case XED_ICLASS_VADDSUBPD:
    /* x87 */
    case XED_ICLASS_FADD:
    case XED_ICLASS_FADDP:
    case XED_ICLASS_FIADD:
    case XED_ICLASS_FCOM:
    case XED_ICLASS_FCOMP:
    case XED_ICLASS_FCOMPP:
    case XED_ICLASS_FCOMI:
    case XED_ICLASS_FCOMIP:
    case XED_ICLASS_FUCOMI:
    case XED_ICLASS_FUCOMIP:
    case XED_ICLASS_FICOM:
    case XED_ICLASS_FICOMP:
    case XED_ICLASS_FSUB:
    case XED_ICLASS_FSUBP:
    case XED_ICLASS_FISUB:
    case XED_ICLASS_FSUBR:
    case XED_ICLASS_FSUBRP:
    case XED_ICLASS_FISUBR:
    case XED_ICLASS_FUCOM:
    case XED_ICLASS_FUCOMP:
    case XED_ICLASS_FUCOMPP:
    case XED_ICLASS_FTST:
        return FU_FADD;
    /* FP multiplies */
    /* SSE */
    case XED_ICLASS_MULPS:
    case XED_ICLASS_MULSS:
    case XED_ICLASS_RCPPS:
    case XED_ICLASS_RCPSS:
    case XED_ICLASS_RSQRTPS:
    case XED_ICLASS_RSQRTSS:
    /* SSE2 */
    case XED_ICLASS_MULPD:
    case XED_ICLASS_MULSD:
    /* SSE4.1 */
    case XED_ICLASS_DPPS:  // the mul is the main uop, the add will follow
    case XED_ICLASS_DPPD:
    /* AVX */
    case XED_ICLASS_VMULPS:
    case XED_ICLASS_VMULSS:
    case XED_ICLASS_VMULPD:
    case XED_ICLASS_VMULSD:
    /* x87 */
    case XED_ICLASS_FMUL:
    case XED_ICLASS_FMULP:
    case XED_ICLASS_FIMUL:
        return FU_FMUL;
    /* FP divides */
    /* SSE */
    case XED_ICLASS_DIVPS:
    case XED_ICLASS_DIVSS:
    /* SSE2 */
    case XED_ICLASS_DIVPD:
    case XED_ICLASS_DIVSD:
    /* AVX */
    case XED_ICLASS_VDIVPS:
    case XED_ICLASS_VDIVSS:
    case XED_ICLASS_VDIVPD:
    case XED_ICLASS_VDIVSD:
    /* x87 */
    case XED_ICLASS_FDIV:
    case XED_ICLASS_FDIVP:
    case XED_ICLASS_FIDIV:
    case XED_ICLASS_FDIVR:
    case XED_ICLASS_FDIVRP:
    case XED_ICLASS_FIDIVR:
    case XED_ICLASS_FPREM:
    case XED_ICLASS_FPREM1:
        return FU_FDIV;
    /* Complex FP operations -- sqrts, transcedentals, etc. */
    /* SSE */
    case XED_ICLASS_SQRTPS:
    case XED_ICLASS_SQRTSS:
    /* SSE2 */
    case XED_ICLASS_SQRTPD:
    case XED_ICLASS_SQRTSD:
    /* AVX */
    case XED_ICLASS_VSQRTPS:
    case XED_ICLASS_VSQRTSS:
    case XED_ICLASS_VSQRTPD:
    case XED_ICLASS_VSQRTSD:
    /* x87 */
    case XED_ICLASS_FCOS:
    case XED_ICLASS_FPATAN:
    case XED_ICLASS_FPTAN:
    case XED_ICLASS_FSIN:
    case XED_ICLASS_FSINCOS:
    case XED_ICLASS_FSQRT:
        return FU_FCPLX;
    default:
        break;
    }

    /* For anything SSE, AVX(2) we haven't captured above, we'll assume
     * a packed integer op.
     * This is blatantly wrong for things like all the CVTXXXX, all horizontal
     * ops, but we'll eventually handle them in the tables above. */
    /* For now, we'll send packed integer ops to the IEU because they finish in a cycle.
     * TODO(skanev): add a packed integer unit on the same ports as the SSE cluster */
    switch (icat) {
    case XED_CATEGORY_SSE:
    case XED_CATEGORY_AVX:
    case XED_CATEGORY_AVX2:
        return FU_IEU;
    default:
        break;
    }

    /* Any other x87 we haven't handled above, we'll assume the add unit. */
    if (uop.decode.is_fpop)
        return FU_FADD;

    /* Finally, default to the integer unit for anything GP, or not handled above. */
    return FU_IEU;
}
}  // xiosim
