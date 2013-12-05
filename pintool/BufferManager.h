#ifndef HANDSHAKE_BUFFER_MANAGER
#define HANDSHAKE_BUFFER_MANAGER

#include "../buffer.h"

class handshake_container_t;

namespace xiosim
{
namespace buffer_management
{
  using boost::interprocess::managed_shared_memory;
  using namespace xiosim::shared;

  void InitBufferManager();
  void DeinitBufferManager();
  bool empty(THREADID tid);
  int AllocateThread(THREADID tid);
  bool hasThread(THREADID tid);
  void signalCallback(int signum);
  void abort(void);

  typedef boost::interprocess::allocator<int, managed_shared_memory::segment_manager> deque_int_allocator;
  typedef boost::interprocess::deque<int, deque_int_allocator> shm_int_deque;
  typedef boost::interprocess::allocator<boost::interprocess::string, managed_shared_memory::segment_manager> deque_allocator;
  typedef boost::interprocess::deque<boost::interprocess::string, deque_allocator> shm_string_deque;

  SHARED_VAR_DECLARE(bool, useRealFile_)
  SHARED_VAR_DECLARE(int, numThreads_)
  SHARED_VAR_DECLARE(bool, popped_)

  extern SharedUnorderedMap<THREADID, XIOSIM_LOCK> locks_;
  extern SharedUnorderedMap<THREADID, int> pool_;
  extern SharedUnorderedMap<THREADID, int64_t> queueSizes_;
  extern SharedUnorderedMap<THREADID, Buffer*> fakeFile_;
  extern SharedUnorderedMap<THREADID, int> fileEntryCount_;
  extern SharedUnorderedMap<THREADID, shm_string_deque> fileNames_;
  extern SharedUnorderedMap<THREADID, shm_int_deque> fileCounts_;

}
}

#endif
