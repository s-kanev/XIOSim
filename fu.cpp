#include "fu.h"

namespace xiosim {

/* enum md_fu_class -> description string */
const char *md_fu2name[NUM_FU_CLASSES] = {
  nullptr, /* NA */
  "int-exec-unit",
  "jump-exec-unit",
  "int-multiply",
  "int-shift",
  "FP-add",
  "FP-multiply",
  "FP-complex",
  "FP-divide",
  "load-port",
  "sta-port",
  "std-port",
  "agen-unit",
  "magic-unit"
};

const char * fu_name(enum md_fu_class fu)
{
    assert(fu < NUM_FU_CLASSES);
    return md_fu2name[fu];
}


}
