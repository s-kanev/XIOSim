#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <stack>
#include <sstream>
#include <map>
#include <queue>

#include "boost_interprocess.h"

#include "../interface.h"
#include "multiprocess_shared.h"
#include "BufferManager.h"

namespace xiosim {
namespace buffer_management {

SHARED_VAR_DEFINE(bool, useRealFile_)
SHARED_VAR_DEFINE(bool, popped_)

SharedUnorderedMap<pid_t, XIOSIM_LOCK> locks_;
SharedUnorderedMap<pid_t, int> pool_;
SharedUnorderedMap<pid_t, int64_t> queueSizes_;
SharedUnorderedMap<pid_t, Buffer*> fakeFile_;  // XXX: fix allocation
SharedUnorderedMap<pid_t, int> fileEntryCount_;
SharedUnorderedMap<pid_t, shm_string_deque> fileNames_;
SharedUnorderedMap<pid_t, shm_int_deque> fileCounts_;

// BufferManager keys.
static const char* BUFFER_MANAGER_LOCKS_ = "buffer_manager_locks";
static const char* BUFFER_MANAGER_POOL_ = "buffer_manager_pool";
static const char* BUFFER_MANAGER_QUEUE_SIZES_ = "buffer_manager_queue_sizes";
static const char* BUFFER_MANAGER_FAKE_FILE_ = "buffer_manager_fake_file";
static const char* BUFFER_MANAGER_FILE_ENTRY_COUNT_ = "buffer_manager_file_entry_count";
static const char* BUFFER_MANAGER_FILE_NAMES_ = "buffer_manager_file_names_";
static const char* BUFFER_MANAGER_FILE_COUNTS_ = "buffer_manager_file_counts";

static int num_cores;

void InitBufferManager(pid_t harness_pid, int num_cores_) {
    using namespace boost::interprocess;
    using namespace xiosim::shared;

    std::stringstream harness_pid_stream;
    harness_pid_stream << harness_pid;
    std::string init_lock_key = harness_pid_stream.str() + std::string(XIOSIM_INIT_SHARED_LOCK);
    named_mutex init_lock(open_only, init_lock_key.c_str());
    init_lock.lock();

    SHARED_VAR_INIT(bool, useRealFile_, true)
    SHARED_VAR_INIT(bool, popped_, false)

    // Reserve space in all maps for 16 cores
    // This reduces the incidence of an annoying race, see
    // comment in empty()
    // This constructor accepts a buckets parameter which negates the need to call
    // reserve on all the maps later.
    locks_.initialize_late(global_shm, BUFFER_MANAGER_LOCKS_, MAX_CORES);
    pool_.initialize_late(global_shm, BUFFER_MANAGER_POOL_, MAX_CORES);

    queueSizes_.initialize_late(global_shm, BUFFER_MANAGER_QUEUE_SIZES_, MAX_CORES);
    fakeFile_.initialize_late(global_shm, BUFFER_MANAGER_FAKE_FILE_, MAX_CORES);
    fileEntryCount_.initialize_late(global_shm, BUFFER_MANAGER_FILE_ENTRY_COUNT_, MAX_CORES);
    fileNames_.initialize_late(global_shm, BUFFER_MANAGER_FILE_NAMES_, MAX_CORES);
    fileCounts_.initialize_late(global_shm, BUFFER_MANAGER_FILE_COUNTS_, MAX_CORES);

    std::cout << "[" << getpid() << "]"
              << "Initialized all SharedUnorderedMaps" << std::endl;
    init_lock.unlock();

    num_cores = num_cores_;
}

void DeinitBufferManager() {}

bool empty(pid_t tid) {
    lk_lock(&locks_[tid], tid + 1);
    bool result = queueSizes_[tid] == 0;
    lk_unlock(&locks_[tid]);
    return result;
}

bool hasThread(pid_t tid) {
    bool result = queueSizes_.count(tid);
    return (result != 0);
}

void cleanBridge(void) {
    std::cerr << "BufferManager cleaning bridge." << std::endl;

    for (auto it_threads = fileNames_.begin(); it_threads != fileNames_.end(); it_threads++)
        for (auto it_files = it_threads->second.begin(); it_files != it_threads->second.end();
             it_files++) {
            unlink(it_files->c_str());
        }
}

int AllocateThread(pid_t tid) {
    *useRealFile_ = true;

    assert(queueSizes_.count(tid) == 0);
    lk_init(&locks_[tid]);
    queueSizes_[tid] = 0;

    fileEntryCount_[tid] = 0;

    int bufferEntries = 640000 / 2;
    int bufferCapacity = bufferEntries / 2 / num_cores;
    if (!*useRealFile_) {
        bufferCapacity /= 8;
        fakeFile_[tid] = new Buffer(120000);
    }

    return bufferCapacity;
}
}
}
