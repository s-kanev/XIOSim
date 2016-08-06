#ifndef _TCM_UTIL_H_
#define _TCM_UTIL_H_
// Utility methods and data structures for the TCMalloc hooks.

#include <list>
#include <string>

#include "xiosim/pintool/feeder.h"
#include "xiosim/pintool/xed_utils.h"

enum MagicInsMode {
    IDEAL,
    REALISTIC,
    BASELINE
};

/* Helper for our actions for each trigger instruction. */
struct magic_insn_action_t {
    /* Instructions to simulate in addition to the trigger instruction. */
    std::list<xed_encoder_instruction_t> insns;
    /* When true, don't simulate the trigger instruction. */
    bool do_replace;

    magic_insn_action_t()
        : insns()
        , do_replace(true) {}

    magic_insn_action_t(std::list<xed_encoder_instruction_t> insns, bool do_replace)
        : insns(insns)
        , do_replace(do_replace) {}
};

using repl_vec_t = std::vector<magic_insn_action_t>;
using insn_vec_t = std::vector<INS>;

// Casts Pin instruction opcode to xed_iclass_enum_t.
inline xed_iclass_enum_t XED_INS_ICLASS(const INS& ins) {
    return static_cast<xed_iclass_enum_t>(INS_Opcode(ins));
};

// Casts Pin instruction opcode to xed_iclass_enum_t.
inline xed_category_enum_t XED_INS_CATEGORY(const INS& ins) {
    return static_cast<xed_category_enum_t>(INS_Category(ins));
};

/* Convert a knob for magic instruction mode into a MagicInsMode enum. */
MagicInsMode StringToMagicInsMode(std::string knob_value);

/* Helper to translate from pin registers to xed registers. Why isn't
 * this exposed through the pin API? */
xed_reg_enum_t PinRegToXedReg(LEVEL_BASE::REG pin_reg);

/* Get next or previous instructions of an iclass or category. */
INS GetNextInsOfClass(const INS& ins, xed_iclass_enum_t iclass);
INS GetPrevInsOfClass(const INS& ins, xed_iclass_enum_t iclass);
INS GetNextInsOfCategory(const INS& ins, xed_category_enum_t category);

INS GetNextZFlagBranch(const INS& ins);

#endif
