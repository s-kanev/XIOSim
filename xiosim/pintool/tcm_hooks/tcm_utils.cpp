#include <string>

#include "xiosim/pintool/xed_utils.h"

#include "tcm_utils.h"

// A function pointer template for either INS_Next() or INS_Prev().
using INS_STEP_FUNC = INS(*)(INS);

xed_reg_enum_t PinRegToXedReg(LEVEL_BASE::REG pin_reg) {
    /* Xed and pin registers are ordered diffently.
     * We'll just go through strings for now, instead of building tables.
     * Yeah, it's ugly. */
    ASSERTX(REG_is_gr(pin_reg));
    std::string pin_str = REG_StringShort(pin_reg);
    for (auto& c : pin_str) c = std::toupper(c);
    auto res = str2xed_reg_enum_t(pin_str.c_str());
    ASSERTX(res != XED_REG_INVALID);
    return res;
}

MagicInsMode StringToMagicInsMode(std::string knob_value) {
    if (knob_value == "ideal") {
        return IDEAL;
    } else if (knob_value == "realistic") {
        return REALISTIC;
    } else if (knob_value == "baseline") {
        return BASELINE;
    } else {
        std::cerr << "Invalid value of magic mode knob: " << knob_value << std::endl;
        abort();
    }
}

/* Helper function to locate the next or prev instruction of a specific iclass.
 *
 * In some cases, we can't find the instruction, in which case the insn will be
 * invalid. The caller is responsible for deciding whether to ASSERTX on this
 * or not.
 */
static INS GetNextOrPrevInsOfClass_(const INS& ins, xed_iclass_enum_t iclass,
                                    INS_STEP_FUNC INS_Step) {
    INS next = INS_Step(ins);
    while (INS_Valid(next)) {
        if (XED_INS_ICLASS(next) == iclass)  // Found it!
            break;
        next = INS_Step(next);
    }
    return next;
}

/* Get the next instruction of an iclass. */
INS GetNextInsOfClass(const INS& ins, xed_iclass_enum_t iclass) {
    return GetNextOrPrevInsOfClass_(ins, iclass, &INS_Next);
}

/* Get the prev instruction of an iclass. */
INS GetPrevInsOfClass(const INS& ins, xed_iclass_enum_t iclass) {
    return GetNextOrPrevInsOfClass_(ins, iclass, &INS_Prev);
}

/* Get the next instuction of a category. */
INS GetNextInsOfCategory(const INS& ins, xed_category_enum_t category) {
    INS next = INS_Next(ins);
    while (INS_Valid(next)) {
        xed_category_enum_t ins_cat = static_cast<xed_category_enum_t>(INS_Category(next));
        if (ins_cat == category) {
            break;
        }
        next = INS_Next(next);
    }
    return next;
}

/* Get the next z-flag branch (je or jne).
 * If the bbl ends with a different control-flow insn, the returned INS is not valid */
INS GetNextZFlagBranch(const INS& ins) {
    INS next = INS_Next(ins);
    xed_iclass_enum_t ins_iclass = XED_ICLASS_INVALID;
    while (INS_Valid(next)) {
        ins_iclass = XED_INS_ICLASS(next);
        if (ins_iclass == XED_ICLASS_JZ || ins_iclass == XED_ICLASS_JNZ)
            break;
        next = INS_Next(next);
    }
    return next;
}
