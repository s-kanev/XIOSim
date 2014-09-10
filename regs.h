/* regs.h - architected register state interfaces */

#ifndef REGS_H
#define REGS_H

extern "C" {
#include "xed-interface.h"
}

#include "host.h"
#include "machine.h"
#include "misc.h"

namespace xiosim {
namespace x86 {

/* number of integer registers */
const size_t NUM_ARCH_IREGS = 8;
const size_t NUM_UARCH_IREGS = 8;
const size_t NUM_IREGS = NUM_ARCH_IREGS + NUM_UARCH_IREGS;
/* size in bytes of interef registers */
const size_t IREG_SIZE = 4;
const size_t IREGS_OFFSET = 0;

/* number of floating point registers */
const size_t NUM_ARCH_FREGS = 8;
const size_t NUM_UARCH_FREGS = 8;
const size_t NUM_FREGS = NUM_ARCH_FREGS + NUM_UARCH_FREGS;
/* size in bytes of fp registers */
const size_t FREG_SIZE = 10;
const size_t FREGS_OFFSET = IREGS_OFFSET + NUM_IREGS;

/* number of XMM registers */
const size_t NUM_ARCH_XMMREGS = 8;
const size_t NUM_UARCH_XMMREGS = 8;
const size_t NUM_XMMREGS = NUM_ARCH_XMMREGS + NUM_UARCH_XMMREGS;
/* size in bytes of XMM registers */
const size_t XMMREG_SIZE = 16;
const size_t XMMREGS_OFFSET = FREGS_OFFSET + NUM_FREGS;

/* number of control registers */
const size_t NUM_CREGS = 4;
const size_t CREGS_OFFSET = XMMREGS_OFFSET + NUM_XMMREGS;

/* number of segment registers */
const size_t NUM_SREGS = 6;
const size_t SREGS_OFFSET = CREGS_OFFSET + NUM_CREGS;

/* total number of registers, excluding PC and NPC */
const size_t NUM_TOTAL_REGS = NUM_IREGS + NUM_FREGS + NUM_XMMREGS + NUM_CREGS + NUM_SREGS;

inline bool is_ireg(xed_reg_enum_t reg) {
    auto reg_class = xed_reg_class(reg);
    return reg_class == XED_REG_CLASS_GPR;
}

inline bool is_freg(xed_reg_enum_t reg) {
    auto reg_class = xed_reg_class(reg);
    return reg_class == XED_REG_CLASS_X87 ||
            reg_class == XED_REG_CLASS_XMM ||
            reg_class == XED_REG_CLASS_YMM;
}

inline bool is_xmmreg(xed_reg_enum_t reg) {
    auto reg_class = xed_reg_class(reg);
    return reg_class == XED_REG_CLASS_XMM;
}

inline bool is_creg(xed_reg_enum_t reg) {
    auto reg_class = xed_reg_class(reg);
    return reg_class == XED_REG_CLASS_FLAGS ||
            reg_class == XED_REG_CLASS_XCR;
}

inline bool is_sreg(xed_reg_enum_t reg) {
    auto reg_class = xed_reg_class(reg);
    return reg_class == XED_REG_CLASS_SR;
}

/* check the tag word for fp register valid bits (physical, not stack index) */
#define FPR_VALID(FTW, N) (((FTW) & (1 << (N))) == (1 << (N)))

/* general purpose (integer) register file entry type */
typedef union {
  dword_t dw[NUM_IREGS];
  struct { word_t lo; word_t hi; } w[NUM_IREGS];
  struct { byte_t lo; byte_t hi; word_t pad; } b[NUM_IREGS];
} md_gpr_t;

// XXX: I don't want to maintain yet another mapping of register names, and for most cases we don't need to
// actually name registers. Hardcode stack location until I figure out a cleaner way.
const size_t REG_ESP = XED_REG_ESP - XED_REG_GPR32_FIRST;
// Hmmm, one more, maybe we need a mapping after all
const size_t REG_EAX = XED_REG_EAX - XED_REG_GPR32_FIRST;

/* floating point register file entry type */
typedef union {
  sfloat_t f[NUM_FREGS][(sizeof(dfloat_t) + sizeof(sfloat_t)) /  sizeof(sfloat_t)];
  struct { dfloat_t lo; sfloat_t hi; } d[NUM_FREGS];
  efloat_t e[NUM_FREGS];    /* extended-precision floating point view */
} md_fpr_t;
/* size in bytes of fp registers */
const size_t FREG_HOST_SIZE = sizeof(md_fpr_t) / NUM_FREGS;

/* segment selector entry type */
typedef union {
  word_t w[NUM_SREGS];
} md_seg_t;

typedef union {
  md_addr_t dw[NUM_SREGS];
} md_seg_base_t;

/* control register file contents */
typedef struct {
  dword_t aflags;        /* processor arithmetic flags */
  word_t cwd;            /* floating point control word */
  word_t fsw;            /* floating point status register */
  byte_t ftw;            /* floating points tag word */
} md_ctrl_t;

/* 128-bit XMM register file */
typedef union {
  sfloat_t f[NUM_XMMREGS][XMMREG_SIZE / sizeof(sfloat_t)];
  struct {dfloat_t lo; dfloat_t hi; } d[NUM_XMMREGS];
  struct {qword_t lo; qword_t hi; } qw[NUM_XMMREGS];
} md_xmm_t;

struct regs_t {
  md_gpr_t regs_R;		/* (signed) integer register file */
  md_fpr_t regs_F;		/* floating point register file */
  md_ctrl_t regs_C;		/* control register file */
  md_addr_t regs_PC;		/* program counter */
  md_addr_t regs_NPC;		/* next-cycle program counter */
  md_seg_t regs_S;  		/* segment register file */
  md_seg_base_t regs_SD;    /* segment bases (part of hidden state) */
  md_xmm_t regs_XMM;        /* 128-bit register file */
};

}
}

#endif /* REGS_H */
