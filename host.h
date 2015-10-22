/* host.h - data-type aliases */

#ifndef HOST_H
#define HOST_H

#include <cstddef>
#include <cstdint>
#include <cinttypes>
#include <limits>

/* statistical counter types, use largest counter type available */
typedef int64_t counter_t;
typedef int64_t tick_t;		/* NOTE: unsigned breaks caches */
const tick_t TICK_T_MAX = std::numeric_limits<tick_t>::max();

typedef uint64_t seq_t;

/* address type definition (depending on build type) */
#ifdef _LP64
typedef uint64_t md_addr_t;
#else
typedef uint32_t md_addr_t;
#endif

/* physical address type definition (64-bit) */
typedef uint64_t md_paddr_t;


#endif /* HOST_H */
