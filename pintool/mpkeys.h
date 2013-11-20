// Keys for locating variables in shared memory via the boost interprocess module.
// Used for multiprogramming support in XIOSim.
//
// Author: Sam Xi

#ifndef MP_KEYS_H
#define MP_KEYS_H

#include <cstdlib>
#include <string>

namespace xiosim {
namespace shared {

// Global shared memory key names.
const std::string XIOSIM_SHARED_MEMORY_KEY = "xiosim_shared_memory";
const std::string XIOSIM_INIT_SHARED_LOCK = "xiosim_init_shared_lock";
const std::string BUFFER_MANAGER_SHARED_MEMORY_KEY = "buffer_manager_shared_memory_key";
const std::string XIOSIM_INIT_COUNTER_KEY = "xiosim_init_counter";

// Shared memory default sizes
const std::size_t DEFAULT_SHARED_MEMORY_SIZE = 65536;

// BufferManager keys.
const std::string BUFFER_MANAGER_LOCKS_ = "buffer_manager_locks";
const std::string BUFFER_MANAGER_POOL_ = "buffer_manager_pool";
const std::string BUFFER_MANAGER_LOGS_ = "buffer_manager_logs";
const std::string BUFFER_MANAGER_QUEUE_SIZES_ = "buffer_manager_queue_sizes";
const std::string BUFFER_MANAGER_FAKE_FILE_ = "buffer_manager_fake_file";
const std::string BUFFER_MANAGER_CONSUME_BUFFER_ = "buffer_manager_consume_buffer";
const std::string BUFFER_MANAGER_PRODUCE_BUFFER_ = "buffer_manager_produce_buffer";
const std::string BUFFER_MANAGER_FILE_ENTRY_COUNT_ = "buffer_manager_file_entry_count";
const std::string BUFFER_MANAGER_FILE_NAMES_ = "buffer_manager_file_names_";
const std::string BUFFER_MANAGER_FILE_COUNTS_ = "buffer_manager_file_counts";
const std::string BUFFER_MANAGER_READ_BUFFER_SIZE_ = "buffer_manager_read_buffer_size";
const std::string BUFFER_MANAGER_READ_BUFFER_ = "buffer_manager_read_buffer";
const std::string BUFFER_MANAGER_WRITE_BUFFER_SIZE_ = "buffer_manager_write_buffer";
const std::string BUFFER_MANAGER_WRITE_BUFFER_ = "buffer_manager_write";


/*
// Shared memory variable typedefs.
typedef allocator<char, managed_shared_memory::segment_manager> CharAllocator;
typedef boost::interprocess::basic_string<char, std::char_traits<char>, CharAllocator> ShmString;
typedef allocator<ShmString, managed_shard_memory::segment_manager> StringAllocator;
typedef deque<ShmString, StringAllcator> ShmDeque;
*/

}  // namespace shared
}  // namespace xiosim

#endif
