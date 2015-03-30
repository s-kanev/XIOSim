/* zesto-repeater.cpp - Ring cache code base classes */
/*
 * Svilen Kanev, 2013
 */

#include "synchronization.h"

#include "zesto-core.h"
#include "zesto-cache.h"
#include "zesto-repeater.h"

extern struct core_knobs_t knobs;

/* Load in definitions */
#include "ZCOMPS-repeater.list"

#define REPEATER_PARSE_ARGS
class repeater_t * repeater_create(
    const char * const opt_string,
    struct core_t * const core,
    const char * const name,
    struct cache_t * const next_level)
{
  char type[255];
  /* the format string "%[^:]" for scanf reads a string consisting of non-':' characters */
  if(sscanf(opt_string,"%[^:]",type) != 1)
    fatal("malformed repeater option string: %s",opt_string);
#include "ZCOMPS-repeater.list"

  fatal("Unknown memory repeater type (%s)", opt_string);
}

#undef REPEATER_PARSE_ARGS

#define REPEATER_INIT
void repeater_init(const char * const opt_string)
{
  char type[255];
  if(sscanf(opt_string,"%[^:]",type) != 1)
    fatal("malformed repeater option string: %s",opt_string);

#include "ZCOMPS-repeater.list"

  fatal("Unknown memory repeater type (%s)", opt_string);
}

#undef REPEATER_INIT

#define REPEATER_SHUTDOWN
void repeater_shutdown(const char * const opt_string)
{
  char type[255];
  if(sscanf(opt_string,"%[^:]",type) != 1)
    fatal("malformed repeater option string: %s",opt_string);
#include "ZCOMPS-repeater.list"

  fatal("Unknown memory repeater type (%s)", opt_string);
}

#undef REPEATER_SHUTDOWN
