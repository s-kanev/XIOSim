/* regs.h - register helper functions */

#ifndef REGS_H
#define REGS_H

extern "C" {
#include "xed-interface.h"
}

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

/* Get the largest architectural register covering @reg.
 * This is different based on 32/64-bit mode:
 * - in 32b, largest_reg(XED_REG_EAX) == XED_REG_EAX
 * - in 64b, largest_reg(XED_REG_EAX) == XED_REG_RAX
 * All explicit register name mentions should be wrapped in this function,
 * so we don't miss dependences due to distinguishing, say EAX/RAX.
 */
inline xed_reg_enum_t largest_reg(const xed_reg_enum_t reg) {
#ifdef _LP64
    return xed_get_largest_enclosing_register(reg);
#else
    return xed_get_largest_enclosing_register32(reg);
#endif
}

}
}

#endif /* REGS_H */
