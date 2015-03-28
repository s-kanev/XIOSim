// Keys for locating variables in shared memory via the boost interprocess
// module.  Used for multiprogramming support in XIOSim.
//
// Author: Sam Xi

#ifndef MP_KEYS_H
#define MP_KEYS_H

#include <cstdlib>

namespace xiosim {
namespace shared {

// Global shared memory key names.
extern const char* XIOSIM_SHARED_MEMORY_KEY;
extern const char* XIOSIM_INIT_SHARED_LOCK;
extern const char* XIOSIM_INIT_COUNTER_KEY;

// Shared memory default sizes
extern const std::size_t DEFAULT_SHARED_MEMORY_SIZE;

}  // namespace shared
}  // namespace xiosim

#endif
