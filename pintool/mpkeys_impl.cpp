#include "mpkeys.h"

namespace xiosim {
namespace shared {

const char *XIOSIM_SHARED_MEMORY_KEY = "xiosim_shared_memory";
const char *XIOSIM_INIT_SHARED_LOCK = "xiosim_init_shared_lock";
const char *XIOSIM_BUFFER_MANAGER_LOCK = "xiosim_buffer_manager_lock";
const char *BUFFER_MANAGER_SHARED_MEMORY_KEY = "buffer_manager_shared_memory_key";
const char *XIOSIM_INIT_COUNTER_KEY = "xiosim_init_counter";

// Shared memory default sizes
const size_t DEFAULT_SHARED_MEMORY_SIZE = 10 * 1024 * 1024;

// BufferManager keys.
const char *BUFFER_MANAGER_LOCKS_ = "buffer_manager_locks";
const char *BUFFER_MANAGER_POOL_ = "buffer_manager_pool";
const char *BUFFER_MANAGER_QUEUE_SIZES_ = "buffer_manager_queue_sizes";
const char *BUFFER_MANAGER_FAKE_FILE_ = "buffer_manager_fake_file";
const char *BUFFER_MANAGER_FILE_ENTRY_COUNT_ = "buffer_manager_file_entry_count";
const char *BUFFER_MANAGER_FILE_NAMES_ = "buffer_manager_file_names_";
const char *BUFFER_MANAGER_FILE_COUNTS_ = "buffer_manager_file_counts";

}
}
