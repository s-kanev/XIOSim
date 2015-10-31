#include <sys/statvfs.h>

#include <unordered_map>
#include <sstream>
#include <mutex>

#include "ipc_queues.h"
#include "buffer.h"
#include "BufferManagerProducer.h"

#include "xiosim/sim.h"

namespace xiosim {
namespace buffer_management {

static void copyProducerToFile(pid_t tid, bool checkSpace);
static void writeHandshake(pid_t tid, int fd, std::string fname, handshake_container_t* handshake);
static int getKBFreeSpace(std::string path);
static std::string genFileName(std::string path);

static std::unordered_map<pid_t, Buffer<handshake_container_t>*> produceBuffer_;
static std::unordered_map<pid_t, int> writeBufferSize_;
static std::unordered_map<pid_t, void*> writeBuffer_;
static std::vector<std::string> bridgeDirs_;
static std::string gpid_;
/* Lock that we capture when allocating a thread. This is the only
 * time we write to any of the unordered maps above. After that,
 * we can just access them lock-free. */
static XIOSIM_LOCK init_lock_;

void InitBufferManagerProducer(pid_t harness_pid) {
    InitBufferManager(harness_pid);

    produceBuffer_.reserve(MAX_CORES);
    writeBufferSize_.reserve(MAX_CORES);
    writeBuffer_.reserve(MAX_CORES);

    bridgeDirs_.push_back("/dev/shm/");
    bridgeDirs_.push_back("/tmp/");

    int pid = getpgrp();
    std::ostringstream iss;
    iss << pid;
    gpid_ = iss.str().c_str();
    assert(gpid_.length() > 0);
    std::cerr << " Creating temp files with prefix " << gpid_ << "_*" << std::endl;
}

void DeinitBufferManagerProducer() { DeinitBufferManager(); }

void AllocateThreadProducer(pid_t tid) {
    std::lock_guard<XIOSIM_LOCK> l(init_lock_);
    int bufferCapacity = AllocateThread(tid);

    produceBuffer_[tid] = new Buffer<handshake_container_t>(bufferCapacity);
    writeBufferSize_[tid] = 4096;
    writeBuffer_[tid] = malloc(4096);
    assert(writeBuffer_[tid]);

    /* send IPC message to allocate consumer-side */
    ipc_message_t msg;
    msg.BufferManagerAllocateThread(tid, bufferCapacity);
    SendIPCMessage(msg);
}

handshake_container_t* Back(pid_t tid) {
    handshake_container_t* returnVal = produceBuffer_[tid]->back();
    return returnVal;
}

/* On the producer side, get a buffer which we can start
 * filling directly.
 */
handshake_container_t* GetBuffer(pid_t tid) {
    // Push is guaranteed to succeed because each call to
    // GetBuffer() is followed by a call to ProducerDone()
    // which will make space if full
    handshake_container_t* result = produceBuffer_[tid]->get_buffer();
    produceBuffer_[tid]->push_done();
    return result;
}

/* On the producer side, signal that we are done filling the
 * current buffer. If we have ran out of space, make space
 * for a new buffer, so GetBuffer() cannot fail.
 */
void ProducerDone(pid_t tid, bool keepLock) {
    assert(!produceBuffer_[tid]->empty());

    /* We've filled the in-memory buffer. Time to flush to a file. */
    if (produceBuffer_[tid]->full()) {
        bool checkSpace = !keepLock;
        copyProducerToFile(tid, checkSpace);
        assert(produceBuffer_[tid]->size() == 0);
    }

    /* Make sure that the next call to GetBuffer() succeeds. */
    assert(!produceBuffer_[tid]->full());
}

/* On the producer side, flush all buffers associated
 * with a thread to the backing file.
 */
void FlushBuffers(pid_t tid) { copyProducerToFile(tid, false); }

bool ProducerEmpty(pid_t tid) { return produceBuffer_[tid]->empty(); }

static void copyProducerToFile(pid_t tid, bool checkSpace) {
    int result;
    bool madeFile = false;
    size_t to_write = produceBuffer_[tid]->size();
    size_t written = 0;
    int bridge_dir_ind = 0;

    if (to_write == 0)
        return;

    /* If we're running out of space (and we care), cycle through
     * bridgeDirs_ until we find one with enough space. If we don't
     * care, we'll just default to the first one. */
    if (checkSpace) {
        for (int i = 0; i < (int)bridgeDirs_.size(); i++) {
            int space = getKBFreeSpace(bridgeDirs_[i]);
            if (space > 1000000) {  // 1.0 GB
                bridge_dir_ind = i;
                madeFile = true;
                break;
            }
            // std::cerr << "Out of space on " + bridgeDirs_[i] + " !!!" << std::endl;
        }
        if (madeFile == false) {
            std::cerr << "Nowhere left for the poor file bridge :(" << std::endl;
            std::cerr << "BridgeDirs:" << std::endl;
            for (int i = 0; i < (int)bridgeDirs_.size(); i++) {
                int space = getKBFreeSpace(bridgeDirs_[i]);
                std::cerr << bridgeDirs_[i] << ":" << space << " in KB" << std::endl;
            }
            abort();
        }
    }

    /* XXX: This is probably a good place to limit producing buffers.
     * Limiting producers only makes sense if we're running low on space,
     * and introduces a lot of non-trivial interactions between the producers
     * and consumers (a lot more locking; and choosing when to stop limiting
     * them to avoid deadlock). So, we don't do it for now, and we hope it
     * doesn't become an issue again with compressed handshakes and 100s of GBs
     * of /dev/shm space. */

    auto filename = genFileName(bridgeDirs_[bridge_dir_ind]);

    int fd = open(filename.c_str(), O_WRONLY | O_CREAT, 0777);
    if (fd == -1) {
        std::cerr << "Opened to write: " << filename;
        std::cerr << "Pipe open error: " << fd << " Errcode:" << strerror(errno) << std::endl;
        abort();
    }

    while (!produceBuffer_[tid]->empty()) {
        writeHandshake(tid, fd, filename, produceBuffer_[tid]->front());
        produceBuffer_[tid]->pop();
        written++;
    }

    result = close(fd);
    if (result != 0) {
        std::cerr << "Close error: "
             << " Errcode:" << strerror(errno) << std::endl;
        abort();
    }

    assert(written >= to_write);

    // sync() if we put the file somewhere besides /dev/shm
    if (filename.find("shm") == std::string::npos) {
        sync();
    }

    /* Everything is written to the file, now we can make it visible to the
     * consumer. */
    NotifyProduced(tid, filename, written);

    assert(produceBuffer_[tid]->size() == 0);
}

static ssize_t do_write(const int fd, const void* buff, const size_t size) {
    ssize_t bytesWritten = 0;
    do {
        ssize_t res = write(fd, (void*)((char*)buff + bytesWritten), size - bytesWritten);
        if (res == -1) {
            std::cerr << "failed write!" << std::endl;
            std::cerr << "bytesWritten:" << bytesWritten << std::endl;
            std::cerr << "size:" << size << std::endl;
            return -1;
        }
        bytesWritten += res;
    } while (bytesWritten < (ssize_t)size);
    return bytesWritten;
}

static void writeHandshake(pid_t tid, int fd, std::string fname, handshake_container_t* handshake) {
    void* writeBuffer = writeBuffer_[tid];
    size_t totalBytes = handshake->Serialize(writeBuffer, 4096);

    ssize_t bytesWritten = do_write(fd, writeBuffer, totalBytes);
    if (bytesWritten == -1) {
        std::cerr << "Pipe write error: " << bytesWritten << " Errcode:" << strerror(errno) << std::endl;

        std::cerr << "Opened to write: " << fname << std::endl;
        std::cerr << "Thread Id:" << tid << std::endl;
        std::cerr << "fd:" << fd << std::endl;
        std::cerr << "ProduceBuffer size:" << produceBuffer_[tid]->size() << std::endl;

        std::cerr << "BridgeDirs:" << std::endl;
        for (int i = 0; i < (int)bridgeDirs_.size(); i++) {
            int space = getKBFreeSpace(bridgeDirs_[i]);
            std::cerr << bridgeDirs_[i] << ":" << space << " in KB" << std::endl;
        }
        abort();
    }
    if (bytesWritten != (ssize_t)totalBytes) {
        std::cerr << "File write error: " << bytesWritten << " expected:" << totalBytes << std::endl;
        std::cerr << fname << std::endl;
        abort();
    }
}

static int getKBFreeSpace(std::string path) {
    struct statvfs fsinfo;
    statvfs(path.c_str(), &fsinfo);
    return ((unsigned long long)fsinfo.f_bsize * (unsigned long long)fsinfo.f_bavail / 1024);
}

static std::string genFileName(std::string path) {
    char* temp = tempnam(path.c_str(), gpid_.c_str());
    std::string res = std::string(temp);
    assert(res.find(path) != std::string::npos);
    res.insert(path.length() + gpid_.length(), "_");
    res = res + ".xiosim";

    free(temp);
    return res;
}
}
}
