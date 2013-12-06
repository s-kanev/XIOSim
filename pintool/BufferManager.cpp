#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/allocators/allocator.hpp>

#include "shared_unordered_map.h"

#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stack>
#include <sstream>
#include <map>
#include <queue>

#include "pin.H"
#include "instlib.H"
#include <stack>
using namespace INSTLIB;

#include "multiprocess_shared.h"
#include "../buffer.h"
#include "BufferManager.h"

namespace xiosim
{
namespace buffer_management
{

SHARED_VAR_DEFINE(bool, useRealFile_)
SHARED_VAR_DEFINE(int, numThreads_)
SHARED_VAR_DEFINE(bool, popped_)

SharedUnorderedMap<pid_t, XIOSIM_LOCK> locks_;
SharedUnorderedMap<pid_t, int> pool_;
SharedUnorderedMap<pid_t, int64_t> queueSizes_;
SharedUnorderedMap<pid_t, Buffer*> fakeFile_; // XXX: fix allocation
SharedUnorderedMap<pid_t, int> fileEntryCount_;
SharedUnorderedMap<pid_t, shm_string_deque> fileNames_;
SharedUnorderedMap<pid_t, shm_int_deque> fileCounts_;

void InitBufferManager()
{
  using namespace boost::interprocess;
  using namespace xiosim::shared;

  named_mutex bm_init_lock(open_or_create, XIOSIM_BUFFER_MANAGER_LOCK);
  bm_init_lock.lock();

  SHARED_VAR_INIT(bool, useRealFile_, true)
  SHARED_VAR_INIT(int, numThreads_, 0)
  SHARED_VAR_INIT(bool, popped_, false)

  // Reserve space in all maps for 16 cores
  // This reduces the incidence of an annoying race, see
  // comment in empty()
  // This constructor accepts a buckets parameter which negates the need to call
  // reserve on all the maps later.
  locks_.initialize_late(XIOSIM_SHARED_MEMORY_KEY, BUFFER_MANAGER_LOCKS_, MAX_CORES);
  pool_.initialize_late(XIOSIM_SHARED_MEMORY_KEY, BUFFER_MANAGER_POOL_, MAX_CORES);

  queueSizes_.initialize_late(XIOSIM_SHARED_MEMORY_KEY, BUFFER_MANAGER_QUEUE_SIZES_, MAX_CORES);
  fakeFile_.initialize_late(XIOSIM_SHARED_MEMORY_KEY, BUFFER_MANAGER_FAKE_FILE_, MAX_CORES);
  fileEntryCount_.initialize_late(XIOSIM_SHARED_MEMORY_KEY, BUFFER_MANAGER_FILE_ENTRY_COUNT_, MAX_CORES);
  fileNames_.initialize_late(XIOSIM_SHARED_MEMORY_KEY, BUFFER_MANAGER_FILE_NAMES_, MAX_CORES);
  fileCounts_.initialize_late(XIOSIM_SHARED_MEMORY_KEY, BUFFER_MANAGER_FILE_COUNTS_, MAX_CORES);

  std::cout << "[" << getpid() << "]" << "Initialized queueSizes" << std::endl;

  bm_init_lock.unlock();
}

void DeinitBufferManager()
{
}

bool empty(pid_t tid)
{
  lk_lock(&locks_[tid], tid+1);
  bool result = queueSizes_[tid] == 0;
  lk_unlock(&locks_[tid]);
  return result;
}

bool hasThread(pid_t tid)
{
  bool result = queueSizes_.count(tid);
  return (result != 0);
}

void abort(){
  signalCallback(SIGABRT);
  exit(1);
  ::abort();
}

void signalCallback(int signum)
{
  cerr << "BufferManager caught signal:" << signum << endl;

  for(auto it_threads = fileNames_.begin(); it_threads != fileNames_.end(); it_threads++)
    for (auto it_files = it_threads->second.begin(); it_files != it_threads->second.end(); it_files++) {
        auto cmd = "/bin/rm -rf " + *it_files + " &";
        int retVal = system(cmd.c_str());
        (void)retVal;
        assert(retVal == 0);
    }
}

int AllocateThread(pid_t tid)
{
  *useRealFile_ = true;

  assert(queueSizes_.count(tid) == 0);
  lk_init(&locks_[tid]);
  queueSizes_[tid] = 0;

  fileEntryCount_[tid] = 0;

  int bufferEntries = 640000 / 2;
  int bufferCapacity = bufferEntries / 2 / KnobNumCores.Value();
  if(!*useRealFile_) {
    bufferCapacity /= 8;
    fakeFile_[tid] = new Buffer(120000);
  }

  *numThreads_ ++;

  return bufferCapacity;
}

}
}
