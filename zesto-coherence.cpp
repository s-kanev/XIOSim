#include <map>

#include "memory.h"
#include "synchronization.h"

#include "zesto-cache.h"
#include "zesto-coherence.h"
#include "zesto-core.h"
#include "zesto-uncore.h"
#include "zesto-noc.h"

using namespace xiosim;

/* load in all definitions */
#include "ZCOMPS-coherence.list"

cache_controller_t::cache_controller_t(struct core_t * const _core, struct cache_t * const _cache) :
  cache(_cache), core(_core)
{}

class cache_controller_t * controller_create(const char * controller_opt_string, struct core_t * core, struct cache_t * cache)
{
#define ZESTO_PARSE_ARGS
#include "ZCOMPS-coherence.list"

  fatal("unknown cache controller type \"%s\"", controller_opt_string);
#undef ZESTO_PARSE_ARGS
}
