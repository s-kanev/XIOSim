/* 
 * Pin-specific declarations, so that we can build Zesto as
 * a library and not link it with pin. 
 * Copyright, Svilen Kanev, 2012
 */

#include <map>
#include <queue>

#include "feeder.h"

// Declared and used in ../synchronization.h
PIN_LOCK memory_lock;
PIN_LOCK cache_lock;
PIN_LOCK cycle_lock;
PIN_LOCK core_pools_lock;
PIN_LOCK oracle_pools_lock;

PIN_LOCK printing_lock;
