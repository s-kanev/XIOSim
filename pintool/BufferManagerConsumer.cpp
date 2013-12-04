#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include "mpkeys.h"

#include <unordered_map>

#include "pin.H"

#include "shared_unordered_map.h"
#include "multiprocess_shared.h"
#include "BufferManagerConsumer.h"

#include "feeder.h"

namespace xiosim
{
namespace buffer_management
{

static void copyFileToConsumer(THREADID tid);
static void copyFileToConsumerReal(THREADID tid);
static void copyFileToConsumerFake(THREADID tid);
static bool readHandshake(THREADID tid, int fd, handshake_container_t* handshake);

static std::unordered_map<THREADID, Buffer*> consumeBuffer_;
static std::unordered_map<THREADID, int> readBufferSize_;
static std::unordered_map<THREADID, void*> readBuffer_;

void InitBufferManagerConsumer()
{
    InitBufferManager();
}

void DeinitBufferManagerConsumer()
{
    DeinitBufferManager();
}

void AllocateThreadConsumer(THREADID tid, int buffer_capacity)
{
    consumeBuffer_[tid] = new Buffer(buffer_capacity);

    // Start with one page read buffer
    readBufferSize_[tid] = 4096;
    readBuffer_[tid] = malloc(4096);
    assert(readBuffer_[tid]);
}

handshake_container_t* front(THREADID tid, bool isLocal)
{

  if(consumeBuffer_[tid]->size() > 0) {
    handshake_container_t* returnVal = consumeBuffer_[tid]->front();
    return returnVal;
  }

  lk_lock(locks_[tid], tid+1);

  assert(queueSizes_[tid] > 0);

  while (fileEntryCount_[tid] == 0) {
    lk_unlock(locks_[tid]);
    while(*consumers_sleep) {
      PIN_SemaphoreWait(consumer_sleep_lock);
      //PIN_Sleep(250);
    }
    PIN_Yield();
    lk_lock(locks_[tid], tid+1);
  }

  if(fileEntryCount_[tid] > 0) {
    copyFileToConsumer(tid);
    assert(!consumeBuffer_[tid]->empty());
    handshake_container_t* returnVal = consumeBuffer_[tid]->front();
    lk_unlock(locks_[tid]);
    return returnVal;
  }
  assert(fileEntryCount_[tid] == 0);
  assert(consumeBuffer_[tid]->empty());

  int spins = 0;
  while(consumeBuffer_[tid]->empty()) {
    lk_unlock(locks_[tid]);
    PIN_Yield();
    while(*consumers_sleep) {
      PIN_SemaphoreWait(consumer_sleep_lock);
    }
    lk_lock(locks_[tid], tid+1);
    spins++;
    if(spins >= 2) {
      spins = 0;
      if(fileEntryCount_[tid] == 0) {
        continue;
      }
      copyFileToConsumer(tid);
    }
  }

  assert(consumeBuffer_[tid]->size() > 0);
  assert(consumeBuffer_[tid]->front()->flags.valid);
  assert(queueSizes_[tid] > 0);
  handshake_container_t* resultVal = consumeBuffer_[tid]->front();
  lk_unlock(locks_[tid]);
  return resultVal;
}

int getConsumerSize(THREADID tid)
{
  return consumeBuffer_[tid]->size();
}

/* On the consumer side, signal that we have consumed
 * numChanged buffers that can go back to the core's pool.
 */
void applyConsumerChanges(THREADID tid, int numChanged)
{
  lk_lock(locks_[tid], tid+1);

  pool_[tid] += numChanged;

  assert(queueSizes_[tid] >= numChanged);
  queueSizes_[tid] -= numChanged;

  lk_unlock(locks_[tid]);
}

void pop(THREADID tid)
{
  consumeBuffer_[tid]->pop();
  *popped_ = true;
  return;
  lk_lock(locks_[tid], tid+1);

  assert(consumeBuffer_[tid]->size() > 0);
  consumeBuffer_[tid]->pop();

  pool_[tid] += 1;
  assert(queueSizes_[tid] > 0);
  queueSizes_[tid] -= 1;

  lk_unlock(locks_[tid]);
}

static void copyFileToConsumer(THREADID tid)
{
  if(useRealFile_) {
    copyFileToConsumerReal(tid);
  }
  else {
    copyFileToConsumerFake(tid);
  }
}

static void copyFileToConsumerFake(THREADID tid)
{
  while(fakeFile_[tid]->size() > 0) {
    if(consumeBuffer_[tid]->full()) {
      break;
    }

    handshake_container_t* handshake = consumeBuffer_[tid]->get_buffer();
    fakeFile_[tid]->front()->CopyTo(handshake);
    consumeBuffer_[tid]->push_done();
    fakeFile_[tid]->pop();
    fileEntryCount_[tid]--;
  }
}

static void copyFileToConsumerReal(THREADID tid)
{
  int result;

  int fd = open(fileNames_[tid].front().c_str(), O_RDWR);
  if(fd == -1) {
    cerr << "Opened to read: " << fileNames_[tid].front();
    cerr << "Pipe open error: " << fd << " Errcode:" << strerror(errno) << endl;
    abort();
  }

  int count = 0;
  bool validRead = true;
  while(fileCounts_[tid].front() > 0) {
    assert(!consumeBuffer_[tid]->full());

    handshake_container_t* handshake = consumeBuffer_[tid]->get_buffer();
    validRead = readHandshake(tid, fd, handshake);
    assert(validRead);
    consumeBuffer_[tid]->push_done();
    count++;
    fileEntryCount_[tid]--;
    fileCounts_[tid].front() -= 1;
  }

  result = close(fd);
  if(result != 0) {
    cerr << "Close error: " << " Errcode:" << strerror(errno) << endl;
    abort();
  }

  assert(fileCounts_[tid].front() == 0);
  result = remove(fileNames_[tid].front().c_str());
  if(result != 0) {
    cerr << "Remove error: " << " Errcode:" << strerror(errno) << endl;
    abort();
  }

  fileNames_[tid].pop_front();
  fileCounts_[tid].pop_front();

  assert(fileEntryCount_[tid] >= 0);
}

static ssize_t do_read(const int fd, void* buff, const size_t size)
{
  ssize_t bytesRead = 0;
  do {
    ssize_t res = read(fd, (void*)((char*)buff + bytesRead), size - bytesRead);
    if(res == -1)
      return -1;
    bytesRead += res;
  } while (bytesRead < (ssize_t)size);
  return bytesRead;
}

static bool readHandshake(THREADID tid, int fd, handshake_container_t* handshake)
{
  const int handshakeBytes = sizeof(P2Z_HANDSHAKE);
  const int flagBytes = sizeof(handshake_flags_t);
  const int mapEntryBytes = sizeof(UINT32) + sizeof(UINT8);

  int mapSize;
  int bytesRead = do_read(fd, &(mapSize), sizeof(int));
  if(bytesRead == -1) {
    cerr << "File read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  assert(bytesRead == sizeof(int));

  int mapBytes = mapSize * mapEntryBytes;
  int totalBytes = handshakeBytes + flagBytes + mapBytes;

  void * readBuffer = readBuffer_[tid];
  assert(readBuffer != NULL);

  bytesRead = do_read(fd, readBuffer, totalBytes);
  if(bytesRead == -1) {
    cerr << "File read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  assert(bytesRead == totalBytes);

  void * buffPosition = readBuffer;
  memcpy(&(handshake->handshake), buffPosition, handshakeBytes);
  buffPosition = (char*)buffPosition + handshakeBytes;

  memcpy(&(handshake->flags), buffPosition, flagBytes);
  buffPosition = (char*)buffPosition + flagBytes;

  handshake->mem_buffer.clear();
  for(int i = 0; i < mapSize; i++) {
    UINT32 first;
    UINT8 second;

    first = *((UINT32*)buffPosition);
    buffPosition = (char*)buffPosition + sizeof(UINT32);

    second = *((UINT8*)buffPosition);
    buffPosition = (char*)buffPosition + sizeof(UINT8);

    (handshake->mem_buffer)[first] = second;
  }

  assert(((unsigned long long int)readBuffer) + totalBytes == ((unsigned long long int)buffPosition));

  return true;
}

}
}
