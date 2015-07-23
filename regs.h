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

}
}

#endif /* REGS_H */
