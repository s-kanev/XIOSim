/* zesto-repeater.cpp - Ring cache code base classes */
/*
 * Svilen Kanev, 2013
 */

#include <cstring>

#include "misc.h"

#include "zesto-core.h"
#include "zesto-cache.h"
#include "zesto-structs.h"
#include "zesto-repeater.h"

/* Load in definitions */
#include "xiosim/ZCOMPS-repeater.list.h"

#define REPEATER_PARSE_ARGS
std::unique_ptr<class repeater_t>  repeater_create(
    const char * const opt_string,
    struct core_t * const core,
    const char * const name,
    struct cache_t * const next_level)
{
  char type[255];
  /* the format string "%[^:]" for scanf reads a string consisting of non-':' characters */
  if(sscanf(opt_string,"%[^:]",type) != 1)
    fatal("malformed repeater option string: %s",opt_string);
#include "xiosim/ZCOMPS-repeater.list.h"

  fatal("Unknown memory repeater type (%s)", opt_string);
}

#undef REPEATER_PARSE_ARGS

#define REPEATER_INIT
void repeater_init(const char * const opt_string)
{
  char type[255];
  if(sscanf(opt_string,"%[^:]",type) != 1)
    fatal("malformed repeater option string: %s",opt_string);

#include "xiosim/ZCOMPS-repeater.list.h"

  fatal("Unknown memory repeater type (%s)", opt_string);
}

#undef REPEATER_INIT

#define REPEATER_SHUTDOWN
void repeater_shutdown(const char * const opt_string)
{
  char type[255];
  if(sscanf(opt_string,"%[^:]",type) != 1)
    fatal("malformed repeater option string: %s",opt_string);
#include "xiosim/ZCOMPS-repeater.list.h"

  fatal("Unknown memory repeater type (%s)", opt_string);
}

#undef REPEATER_SHUTDOWN
