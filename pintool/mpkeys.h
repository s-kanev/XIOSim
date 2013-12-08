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
extern const char* BUFFER_MANAGER_CONSUME_BUFFER_;
extern const char* BUFFER_MANAGER_PRODUCE_BUFFER_;
extern const char* BUFFER_MANAGER_FILE_ENTRY_COUNT_;
extern const char* BUFFER_MANAGER_FILE_NAMES_;
extern const char* BUFFER_MANAGER_FILE_COUNTS_;
extern const char* BUFFER_MANAGER_READ_BUFFER_SIZE_;
extern const char* BUFFER_MANAGER_READ_BUFFER_;
extern const char* BUFFER_MANAGER_WRITE_BUFFER_SIZE_;
extern const char* BUFFER_MANAGER_WRITE_BUFFER_;

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
