// Keys for locating variables in shared memory via the boost interprocess module.
// Used for multiprogramming support in XIOSim.
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
extern const char *XIOSIM_BUFFER_MANAGER_LOCK;
extern const char* BUFFER_MANAGER_SHARED_MEMORY_KEY;
extern const char* XIOSIM_INIT_COUNTER_KEY;

// Shared memory default sizes
extern const std::size_t DEFAULT_SHARED_MEMORY_SIZE;

// BufferManager keys.
extern const char* BUFFER_MANAGER_LOCKS_;
extern const char* BUFFER_MANAGER_POOL_;
extern const char* BUFFER_MANAGER_QUEUE_SIZES_;
extern const char* BUFFER_MANAGER_FAKE_FILE_;
extern const char* BUFFER_MANAGER_FILE_ENTRY_COUNT_;
extern const char* BUFFER_MANAGER_FILE_NAMES_;
extern const char* BUFFER_MANAGER_FILE_COUNTS_;

}  // namespace shared
}  // namespace xiosim

#endif
