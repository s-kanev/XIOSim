#ifndef HANDSHAKE_BUFFER_MANAGER
#define HANDSHAKE_BUFFER_MANAGER

#include "buffer.h"

class handshake_container_t;

namespace xiosim {
namespace buffer_management {
using boost::interprocess::managed_shared_memory;
using boost::interprocess::allocator;
using namespace xiosim::shared;

void InitBufferManager(pid_t harness_pid, int num_cores);
void DeinitBufferManager();
bool empty(pid_t tid);
int AllocateThread(pid_t tid);
bool hasThread(pid_t tid);
void cleanBridge(void);

typedef allocator<int, managed_shared_memory::segment_manager> int_allocator;
typedef boost::interprocess::deque<int, int_allocator> shm_int_deque;
typedef allocator<char, managed_shared_memory::segment_manager> char_allocator;
typedef boost::interprocess::basic_string<char, std::char_traits<char>, char_allocator> shm_string;
typedef allocator<shm_string, managed_shared_memory::segment_manager> shm_string_allocator;
typedef boost::interprocess::deque<shm_string, shm_string_allocator> shm_string_deque;

SHARED_VAR_DECLARE(bool, useRealFile_)
SHARED_VAR_DECLARE(bool, popped_)

extern SharedUnorderedMap<pid_t, XIOSIM_LOCK> locks_;
extern SharedUnorderedMap<pid_t, int> pool_;
extern SharedUnorderedMap<pid_t, int64_t> queueSizes_;
extern SharedUnorderedMap<pid_t, Buffer*> fakeFile_;
extern SharedUnorderedMap<pid_t, int> fileEntryCount_;
extern SharedUnorderedMap<pid_t, shm_string_deque> fileNames_;
extern SharedUnorderedMap<pid_t, shm_int_deque> fileCounts_;
}
}

#endif
