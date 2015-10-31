#include <fcntl.h>
#include <unordered_map>
#include <mutex>

#include "xiosim/synchronization.h"

#include "buffer.h"
#include "BufferManager.h"

#include "BufferManagerConsumer.h"

using namespace std;

namespace xiosim {
namespace buffer_management {

static void copyFileToConsumer(pid_t tid, std::string fname, size_t to_read);
static bool readHandshake(pid_t tid, int fd, handshake_container_t* handshake);

static std::unordered_map<pid_t, Buffer<handshake_container_t>*> consumeBuffer_;
static std::unordered_map<pid_t, int> readBufferSize_;
static std::unordered_map<pid_t, void*> readBuffer_;
/* Lock that we capture when allocating a thread. This is the only
 * time we write to any of the unordered maps above. After that,
 * we can just access them lock-free. */
static XIOSIM_LOCK init_lock_;

void InitBufferManagerConsumer(pid_t harness_pid) {
    InitBufferManager(harness_pid);
}

void DeinitBufferManagerConsumer() { DeinitBufferManager(); }

void AllocateThreadConsumer(pid_t tid, int buffer_capacity) {
    std::lock_guard<XIOSIM_LOCK> l(init_lock_);
    // Start with one page read buffer
    readBufferSize_[tid] = 4096;
    readBuffer_[tid] = malloc(4096);
    assert(readBuffer_[tid]);

    consumeBuffer_[tid] = new Buffer<handshake_container_t>(buffer_capacity);
}

handshake_container_t* Front(pid_t tid) {
    /* Fast path, we have a handshake in our local buffer.
     * Just return it without touching any locks. */
    assert(consumeBuffer_[tid] != NULL);
    if (consumeBuffer_[tid]->size() > 0) {
        handshake_container_t* returnVal = consumeBuffer_[tid]->front();
        return returnVal;
    }

    /* consumeBuffer_ is empty. We need to read from a file.
     * First, wait until one exists, and is flushed by producers. */
    auto new_file = WaitForFile(tid);

    /* Now that we have a file, copy it to consumeBuffer_. */
    copyFileToConsumer(tid, new_file.first, new_file.second);
    assert(!consumeBuffer_[tid]->empty());
    return consumeBuffer_[tid]->front();
}

int GetConsumerSize(pid_t tid) {
    // Another thread might be doing the allocation
    while (consumeBuffer_[tid] == NULL)
        ;

    assert(consumeBuffer_[tid] != NULL);
    return consumeBuffer_[tid]->size();
}

void Pop(pid_t tid) { consumeBuffer_[tid]->pop(); }

/* Read @to_read handshake buffers from file @fname for thread @tid. */
static void copyFileToConsumer(pid_t tid, std::string fname, size_t to_read) {
    int result;

    int fd = open(fname.c_str(), O_RDWR);
    if (fd == -1) {
        cerr << "Opened to read: " << fname;
        cerr << "Pipe open error: " << fd << " Errcode:" << strerror(errno) << endl;
        abort();
    }

    size_t num_read = 0;
    bool validRead = true;
    while (to_read > 0) {
        assert(!consumeBuffer_[tid]->full());

        handshake_container_t* handshake = consumeBuffer_[tid]->get_buffer();
        validRead = readHandshake(tid, fd, handshake);
#ifdef NDEBUG
        (void)validRead;
#endif
        assert(validRead);
        consumeBuffer_[tid]->push_done();
        num_read++;
        to_read--;
    }

    result = close(fd);
    if (result != 0) {
        cerr << "Close error: "
             << " Errcode:" << strerror(errno) << endl;
        abort();
    }

    result = remove(fname.c_str());
    if (result != 0) {
        cerr << "Remove error: "
             << " Errcode:" << strerror(errno) << endl;
        abort();
    }

    /* Let the BufferManager know we are done consuming this file. */
    NotifyConsumed(tid, num_read);
}

static ssize_t do_read(const int fd, void* buff, const size_t size) {
    ssize_t bytesRead = 0;
    do {
        ssize_t res = read(fd, (void*)((char*)buff + bytesRead), size - bytesRead);
        if (res == -1)
            return -1;
        bytesRead += res;
    } while (bytesRead < (ssize_t)size);
    return bytesRead;
}

static bool readHandshake(pid_t tid, int fd, handshake_container_t* handshake) {
    ssize_t bufferSize;
    ssize_t bytesRead = do_read(fd, &(bufferSize), sizeof(ssize_t));
    if (bytesRead == -1) {
        cerr << "File read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
        abort();
    }
    assert(bytesRead == sizeof(size_t));

    /* We have read the size field already. Now read the rest. */
    bufferSize -= sizeof(bufferSize);

    void* readBuffer = readBuffer_[tid];
    assert(readBuffer != NULL);

    bytesRead = do_read(fd, readBuffer, bufferSize);
    if (bytesRead == -1) {
        cerr << "File read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
        abort();
    }
    assert(bytesRead == bufferSize);

    handshake->Deserialize(readBuffer, bufferSize);
    return true;
}
}
}
