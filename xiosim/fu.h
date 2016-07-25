#ifndef __FU_H__
#define __FU_H__

#include <string>

/* Forward declaration, zesto-structs.h depends on this file. */
struct uop_t;

namespace xiosim {

/* functional unit classes */
enum fu_class {
    FU_INVALID = -1,
    FU_NA = 0,     /* inst does not use a functional unit */
    FU_IEU,        /* integer ALU */
    FU_JEU,        /* jump execution unit */
    FU_IMUL,       /* integer multiplier  */
    FU_IDIV,       /* integer divider */
    FU_SHIFT,      /* integer shifter */
    FU_FADD,       /* floating point adder */
    FU_FMUL,       /* floating point multiplier */
    FU_FDIV,       /* floating point divider */
    FU_FCPLX,      /* floating point complex (sqrt,transcendentals,...) */
    FU_LD,         /* load port/AGU */
    FU_STA,        /* store-address port/AGU */
    FU_STD,        /* store-data port */
    FU_AGEN,       /* AGU used for LEA and int<>float forwarding */
    FU_MAGIC,      /* Magic ALU used to model accelerators */
    FU_SAMPLING,
    NUM_FU_CLASSES /* total functional unit classes */
};

std::string fu_name(enum fu_class fu);

fu_class get_uop_fu(const struct uop_t& uop);
}  // xiosim

#endif /* __FU_H__ */
