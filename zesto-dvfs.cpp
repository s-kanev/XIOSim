#include "zesto-core.h"
#include "zesto-dvfs.h"

/* load in all definitions */
#include "ZCOMPS-dvfs.list"

class vf_controller_t * vf_controller_create(const char * opt_string, struct core_t * core)
{
#define ZESTO_PARSE_ARGS
#include "ZCOMPS-dvfs.list"
#undef ZESTO_PARSE_ARGS

  fatal("unknown dvfs controller type (%s)", opt_string);
}
