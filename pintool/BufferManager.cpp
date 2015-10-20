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

#include "multiprocess_shared.h"
#include "../sim.h"
#include "BufferManager.h"

namespace xiosim {
namespace buffer_management {
using namespace boost::interprocess;
using namespace xiosim::shared;

typedef allocator<char, managed_shared_memory::segment_manager> char_allocator;
typedef boost::interprocess::basic_string<char, std::char_traits<char>, char_allocator> shm_string;
typedef allocator<shm_string, managed_shared_memory::segment_manager> shm_string_allocator;

/* A per-thread instance of fileBuffer. Lives in shared memory, so both producers
 * and consumers can access it. Keeps a list of the files (which themselves are typically
 * in /dev/shm) that make up for the buffer, and each file's size. */
class FileBuffer {
    typedef allocator<int, managed_shared_memory::segment_manager> int_allocator;
    typedef boost::interprocess::deque<int, int_allocator> shm_int_deque;
    typedef boost::interprocess::deque<shm_string, shm_string_allocator> shm_string_deque;
    typedef boost::interprocess::allocator<void, managed_shared_memory::segment_manager>
        VoidAllocator;

  public:
    FileBuffer(const VoidAllocator& allocator)
        : fileEntryCount(0)
        , fileNames(shm_string_allocator(allocator.get_segment_manager()))
        , fileCounts(int_allocator(allocator.get_segment_manager())) {}

    /* Move constructor, so we can easily put in a SharedUnorderedMap. */
    FileBuffer(FileBuffer&& arg)
        : fileEntryCount(arg.fileEntryCount)
        , fileNames(std::move(arg.fileNames))
        , fileCounts(std::move(arg.fileCounts))
        , lock(std::move(arg.lock)) {}
    /*        , cv(std::move(arg.cv)) { }
     *     XXX: interprocess_condition_any isn't movable. This would reconstruct a
     *     fresh one. Which is ok for now, because FileBuffer is only emplaced once.
     *     but is all kinds of evil in the long run.
    */

    /* How many elements in total in this buffer. */
    int fileEntryCount;
    /* A list of the files that make up the buffer. */
    shm_string_deque fileNames;
    /* How many entries in each file. */
    shm_int_deque fileCounts;

    /* Protects the above. Always captured on reads and writes.
     * XXX: Not contended any more. If it ever becomes again, we can make
     * at least some of the reads lock-free. */
    XIOSIM_LOCK lock;
    /* CV that we use on the consumer side to wait until an entry shows up. */
    boost::interprocess::interprocess_condition_any cv;

  private:
    /* Remove default constructor. We need an allocator to live in shm. */
    FileBuffer() = delete;
    /* Remove copy constructor. */
    FileBuffer(const FileBuffer&) = delete;
};

/* Per-thread buffer that lives in shared memory. */
typedef SharedUnorderedMap<pid_t, FileBuffer> FileBufferMap;
SHARED_VAR_DEFINE(FileBufferMap, fileBuffer)

/* Chosen by manual tunning. */
static const int bufferCapacity = 100000;

/* Helper to check if per-thread entry has been created. */
static bool hasThread(pid_t tid) { return (fileBuffer->count(tid) > 0); }

/* Clean up all files in all buffers. */
static void cleanBridge(void) {
    std::cerr << "BufferManager cleaning bridge." << std::endl;

    for (auto& buffer_pair : *fileBuffer) {
        auto& buffer = buffer_pair.second;
        scoped_lock<XIOSIM_LOCK> l(buffer.lock);
        for (auto& fname : buffer.fileNames)
            unlink(fname.c_str());
    }
}

void InitBufferManager(pid_t harness_pid) {
    std::stringstream harness_pid_stream;
    harness_pid_stream << harness_pid;
    std::string init_lock_key = harness_pid_stream.str() + std::string(XIOSIM_INIT_SHARED_LOCK);
    named_mutex init_lock(open_only, init_lock_key.c_str());
    init_lock.lock();

    SHARED_VAR_CONSTRUCT(FileBufferMap, fileBuffer, MAX_CORES);

    init_lock.unlock();
}

void DeinitBufferManager() { cleanBridge(); }

int AllocateThread(pid_t tid) {
    assert(!hasThread(tid));

    /* Create a new entry in fileBuffer */
    fileBuffer->operator[](tid);

    return bufferCapacity;
}

void NotifyProduced(pid_t tid, std::string filename, size_t n_items) {
    FileBuffer& buffer = fileBuffer->at(tid);

    shm_string fname(filename.c_str(), global_shm->get_allocator<shm_string>());

    scoped_lock<XIOSIM_LOCK> l(buffer.lock);
    buffer.fileNames.push_back(fname);
    buffer.fileCounts.push_back(n_items);
    buffer.fileEntryCount += n_items;
    buffer.cv.notify_all();
}

std::pair<std::string, size_t> WaitForFile(pid_t tid) {
    FileBuffer& buffer = fileBuffer->at(tid);

    scoped_lock<XIOSIM_LOCK> l(buffer.lock);
    while (buffer.fileEntryCount == 0)
        buffer.cv.wait(l);

    return std::make_pair(buffer.fileNames.front().c_str(), buffer.fileCounts.front());
}

void NotifyConsumed(pid_t tid, size_t n_items) {
    FileBuffer& buffer = fileBuffer->at(tid);

    scoped_lock<XIOSIM_LOCK> l(buffer.lock);
    buffer.fileEntryCount -= n_items;
    buffer.fileNames.pop_front();
    buffer.fileCounts.pop_front();
    assert(buffer.fileEntryCount >= 0);
}
}
}
