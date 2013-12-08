#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include "mpkeys.h"

#include <sys/statvfs.h>

#include <unordered_map>
#include <sstream>

#include "pin.H"

#include "shared_unordered_map.h"
#include "multiprocess_shared.h"
#include "ipc_queues.h"
#include "BufferManagerProducer.h"

#include "feeder.h"

namespace xiosim
{
namespace buffer_management
{

static void copyProducerToFile(pid_t tid, bool checkSpace);
static void copyProducerToFileReal(pid_t tid, bool checkSpace);
static void copyProducerToFileFake(pid_t tid);
static void writeHandshake(pid_t tid, int fd, handshake_container_t* handshake);
static int getKBFreeSpace(boost::interprocess::string path);
static void reserveHandshake(pid_t tid);
static shm_string genFileName(boost::interprocess::string path);

static std::unordered_map<pid_t, Buffer*> produceBuffer_;
static std::unordered_map<pid_t, int> writeBufferSize_;
static std::unordered_map<pid_t, void*> writeBuffer_;
static vector<boost::interprocess::string> bridgeDirs_;
static boost::interprocess::string gpid_;

void InitBufferManagerProducer(void)
{
    InitBufferManager();

    produceBuffer_.reserve(MAX_CORES);
    writeBufferSize_.reserve(MAX_CORES);
    writeBuffer_.reserve(MAX_CORES);

    bridgeDirs_.push_back("/dev/shm/");
    bridgeDirs_.push_back("/tmp/");
    bridgeDirs_.push_back("./");

    int pid = getpgrp();
    ostringstream iss;
    iss << pid;
    gpid_ = iss.str().c_str();
    assert(gpid_.length() > 0);
    cerr << " Creating temp files with prefix "  << gpid_ << "_*" << endl;
}

void DeinitBufferManagerProducer()
{
    DeinitBufferManager();

    for(int i = 0; i < (int)bridgeDirs_.size(); i++) {
        boost::interprocess::string cmd = "/bin/rm -rf " + bridgeDirs_[i] + gpid_ + "_* &";
        int retVal = system(cmd.c_str());
        (void)retVal;
        assert(retVal == 0);
    }
}

void AllocateThreadProducer(pid_t tid)
{
    int bufferCapacity = AllocateThread(tid);

    produceBuffer_[tid] = new Buffer(bufferCapacity);
    resetPool(tid);
    writeBufferSize_[tid] = 4096;
    writeBuffer_[tid] = malloc(4096);
    assert(writeBuffer_[tid]);

    /* send IPC message to allocate consumer-side */
    ipc_message_t msg;
    msg.BufferManagerAllocateThread(tid, bufferCapacity);
    SendIPCMessage(msg);
}

handshake_container_t* back(pid_t tid)
{
    lk_lock(&locks_[tid], tid+1);
    assert(queueSizes_[tid] > 0);
    handshake_container_t* returnVal = produceBuffer_[tid]->back();
    lk_unlock(&locks_[tid]);
    return returnVal;
}

/* On the producer side, get a buffer which we can start
 * filling directly.
 */
handshake_container_t* get_buffer(pid_t tid)
{
  lk_lock(&locks_[tid], tid+1);
  // Push is guaranteed to succeed because each call to
  // get_buffer() is followed by a call to producer_done()
  // which will make space if full
  handshake_container_t* result = produceBuffer_[tid]->get_buffer();
  produceBuffer_[tid]->push_done();
  queueSizes_[tid]++;
  assert(pool_[tid] > 0);
  pool_[tid]--;

  lk_unlock(&locks_[tid]);
  return result;
}

/* On the producer side, signal that we are done filling the
 * current buffer. If we have ran out of space, make space
 * for a new buffer, so get_buffer() cannot fail.
 */
void producer_done(pid_t tid, bool keepLock)
{
  lk_lock(&locks_[tid], tid+1);

  ASSERTX(!produceBuffer_[tid]->empty());
  handshake_container_t* last = produceBuffer_[tid]->back();
  ASSERTX(last->flags.valid);

  if(!keepLock) {
    reserveHandshake(tid);
  }

  if(produceBuffer_[tid]->full()) {// || ( (consumeBuffer_[tid]->size() == 0) && (fileEntryCount_[tid] == 0))) {
#if defined(DEBUG) || defined(ZESTO_PIN_DBG)
    int produceSize = produceBuffer_[tid]->size();
#endif
    bool checkSpace = !keepLock;
    copyProducerToFile(tid, checkSpace);
    assert(fileEntryCount_[tid] > 0);
    assert(fileEntryCount_[tid] >= produceSize);
    assert(produceBuffer_[tid]->size() == 0);
  }

  assert(!produceBuffer_[tid]->full());

  lk_unlock(&locks_[tid]);
}

/* On the producer side, flush all buffers associated
 * with a thread to the backing file.
 */
void flushBuffers(pid_t tid)
{
  lk_lock(&locks_[tid], tid+1);

  if(produceBuffer_[tid]->size() > 0) {
    copyProducerToFile(tid, false);
  }
  lk_unlock(&locks_[tid]);
}

void resetPool(pid_t tid)
{
  int poolFactor = 1;
  if(KnobNumCores.Value() > 1) {
    poolFactor = 6;
  }
  /* Assume produceBuffer->capacity == consumeBuffer->capacity */
  pool_[tid] = (2 * produceBuffer_[tid]->capacity()) * poolFactor;
  //  pool_[tid] = 2000000000;
}

/* On the producer side, if we have filled up the in-memory
 * buffer, wait until some of it gets consumed. If not,
 * try and increase the backing file size.
 */
static void reserveHandshake(pid_t tid)
{
  int64_t queueLimit;
  if(KnobNumCores.Value() > 1) {
    queueLimit = 5000000001;
  }
  else {
    queueLimit = 5000000001;
  }

  if(pool_[tid] > 0) {
    return;
  }

  //  while(pool_[tid] == 0) {
  while(true) {
    assert(queueSizes_[tid] > 0);
    lk_unlock(&locks_[tid]);

    enable_consumers();
    disable_producers();

    PIN_Sleep(1000);

    lk_lock(&locks_[tid], tid+1);

    if(*popped_) {
      *popped_ = false;
      continue;
    }

    disable_consumers();
    enable_producers();

    //    if(num_cores == 1 || (!*useRealFile_)) {
    //      continue;
    //    }

    if(queueSizes_[tid] < queueLimit) {
      pool_[tid] += 50000;
#ifdef ZESTO_PIN_DBG
      cerr << tid << " [reserveHandshake()]: Increasing file up to " << queueSizes_[tid] + pool_[tid] << endl;
#endif
      break;
    }
    cerr << tid << " [reserveHandshake()]: File size too big to expand, abort():" << queueSizes_[tid] << endl;
    abort();
  }
}

static void copyProducerToFile(pid_t tid, bool checkSpace)
{
  if(*useRealFile_) {
    copyProducerToFileReal(tid, checkSpace);
  }
  else {
    copyProducerToFileFake(tid);
  }
}

static void copyProducerToFileFake(pid_t tid)
{
  while(produceBuffer_[tid]->size() > 0) {
    handshake_container_t* handshake = produceBuffer_[tid]->front();
    handshake_container_t* handfake = fakeFile_[tid]->get_buffer();
    handshake->CopyTo(handfake);
    fakeFile_[tid]->push_done();

    produceBuffer_[tid]->pop();
    fileEntryCount_[tid]++;
  }
}

static void copyProducerToFileReal(pid_t tid, bool checkSpace)
{
  int result;
  bool madeFile = false;
  if(checkSpace) {
    for(int i = 0; i < (int)bridgeDirs_.size(); i++) {
      int space = getKBFreeSpace(bridgeDirs_[i]);
      if(space > 2000000) { // 2 GB
        fileNames_[tid].push_back(genFileName(bridgeDirs_[i]));
        madeFile = true;
        break;
      }
      //cerr << "Out of space on " + bridgeDirs_[i] + " !!!" << endl;
    }
    if(madeFile == false) {
      cerr << "Nowhere left for the poor file bridge :(" << endl;
      abort();
    }
  }
  else {
    fileNames_[tid].push_back(genFileName(bridgeDirs_[0]));
  }

  fileCounts_[tid].push_back(0);

  int fd = open(fileNames_[tid].back().c_str(), O_WRONLY | O_CREAT, 0777);
  if(fd == -1) {
    cerr << "Opened to write: " << fileNames_[tid].back();
    cerr << "Pipe open error: " << fd << " Errcode:" << strerror(errno) << endl;
    abort();
  }

  while(!produceBuffer_[tid]->empty()) {
    writeHandshake(tid, fd, produceBuffer_[tid]->front());
    produceBuffer_[tid]->pop();
    fileCounts_[tid].back() += 1;
    fileEntryCount_[tid]++;
  }

  result = close(fd);
  if(result != 0) {
    cerr << "Close error: " << " Errcode:" << strerror(errno) << endl;
    abort();
  }

  // sync() if we put the file somewhere besides /dev/shm
  if(fileNames_[tid].back().find("shm") == shm_string::npos) {
    sync();
  }

  assert(produceBuffer_[tid]->size() == 0);
  assert(fileEntryCount_[tid] >= 0);
}

static ssize_t do_write(const int fd, const void* buff, const size_t size)
{
  ssize_t bytesWritten = 0;
  do {
    ssize_t res = write(fd, (void*)((char*)buff + bytesWritten), size - bytesWritten);
    if(res == -1)
      return -1;
    bytesWritten += res;
  } while (bytesWritten < (ssize_t)size);
  return bytesWritten;
}

static void writeHandshake(pid_t tid, int fd, handshake_container_t* handshake)
{
  int mapSize = handshake->mem_buffer.size();
  const int handshakeBytes = sizeof(P2Z_HANDSHAKE);
  const int flagBytes = sizeof(handshake_flags_t);
  const int mapEntryBytes = sizeof(UINT32) + sizeof(UINT8);
  int mapBytes = mapSize * mapEntryBytes;
  int totalBytes = sizeof(int) + handshakeBytes + flagBytes + mapBytes;

  assert(totalBytes <= 4096);

  void * writeBuffer = writeBuffer_[tid];
  void * buffPosition = writeBuffer;

  memcpy((char*)buffPosition, &(mapSize), sizeof(int));
  buffPosition = (char*)buffPosition + sizeof(int);

  memcpy((char*)buffPosition, &(handshake->handshake), handshakeBytes);
  buffPosition = (char*)buffPosition + handshakeBytes;

  memcpy((char*)buffPosition, &(handshake->flags), flagBytes);
  buffPosition = (char*)buffPosition + flagBytes;

  std::map<UINT32, UINT8>::iterator it;
  for(it = handshake->mem_buffer.begin(); it != handshake->mem_buffer.end(); it++) {
    memcpy((char*)buffPosition, &(it->first), sizeof(UINT32));
    buffPosition = (char*)buffPosition + sizeof(UINT32);

    memcpy((char*)buffPosition, &(it->second), sizeof(UINT8));
    buffPosition = (char*)buffPosition + sizeof(UINT8);
  }

  assert(((unsigned long long int)writeBuffer) + totalBytes == ((unsigned long long int)buffPosition));

  int bytesWritten = do_write(fd, writeBuffer, totalBytes);
  if(bytesWritten == -1) {
    cerr << "Pipe write error: " << bytesWritten << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  if(bytesWritten != totalBytes) {
    cerr << "File write error: " << bytesWritten << " expected:" << totalBytes << endl;
    cerr << fileNames_[tid].back() << endl;
    abort();
  }
}

static int getKBFreeSpace(boost::interprocess::string path)
{
  struct statvfs fsinfo;
  statvfs(path.c_str(), &fsinfo);
  return (fsinfo.f_bsize * fsinfo.f_bfree / 1024);
}

static shm_string genFileName(boost::interprocess::string path)
{
  char* temp = tempnam(path.c_str(), gpid_.c_str());
  boost::interprocess::string res = boost::interprocess::string(temp);
  assert(res.find(path) != boost::interprocess::string::npos);
  res.insert(path.length() + gpid_.length(), "_");
  res = res + ".xiosim";

  shm_string shared_res(res.c_str(), global_shm->get_allocator<void>());
  free(temp);
  return shared_res;
}

}
}
