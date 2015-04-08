#include "mpkeys.h"

namespace xiosim {
namespace shared {

const char* XIOSIM_SHARED_MEMORY_KEY = "xiosim_shared_memory";
const char* XIOSIM_INIT_SHARED_LOCK = "xiosim_init_shared_lock";
const char* XIOSIM_INIT_COUNTER_KEY = "xiosim_init_counter";

// Shared memory default sizes
const size_t DEFAULT_SHARED_MEMORY_SIZE = 10 * 1024 * 1024;
}
}
