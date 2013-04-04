/* zesto-repeater.cpp - Ring cache code base classes */
/*
 * Svilen Kanev, 2013
 */

#include "synchronization.h"

#include "zesto-core.h"
#include "zesto-cache.h"
#include "zesto-repeater.h"

#include "ZCOMPS-repeater.list"

#define REPEATER_PARSE_ARGS
class repeater_t * repeater_create(
    const char * const opt_string,
    struct core_t * const core,
    const char * const name,
    struct cache_t * const next_level)
{
#include "ZCOMPS-repeater.list"

  fatal("Unknown memory repeater type (%s)", opt_string);
}

#undef REPEATER_PARSE_ARGS

#define REPEATER_PARSE_OPTIONS
void repeater_reg_options(struct opt_odb_t * const odb, const char * const opt_string)
{
#include "ZCOMPS-repeater.list"

  fatal("Unknown memory repeater type (%s)", opt_string);
}

#undef REPEATER_PARSE_OPTIONS

#define REPEATER_INIT
void repeater_init(const char * const opt_string)
{
#include "ZCOMPS-repeater.list"

  fatal("Unknown memory repeater type (%s)", opt_string);
}

#undef REPEATER_INIT

#define REPEATER_SHUTDOWN
void repeater_shutdown(const char * const opt_string)
{
#include "ZCOMPS-repeater.list"

  fatal("Unknown memory repeater type (%s)", opt_string);
}

#undef REPEATER_SHUTDOWN
