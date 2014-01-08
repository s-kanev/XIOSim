#include <unordered_map>

#include "pin.H"

#include "boost_interprocess.h"

#include "../interface.h"
#include "multiprocess_shared.h"
#include "BufferManagerConsumer.h"

namespace xiosim
{
namespace buffer_management
{

static void copyFileToConsumer(pid_t tid);
static void copyFileToConsumerReal(pid_t tid);
static void copyFileToConsumerFake(pid_t tid);
static bool readHandshake(pid_t tid, int fd, handshake_container_t* handshake);

static std::unordered_map<pid_t, Buffer*> consumeBuffer_;
static std::unordered_map<pid_t, int> readBufferSize_;
static std::unordered_map<pid_t, void*> readBuffer_;

VOID ConsumerSignalCallback(THREADID threadIndex, CONTEXT_CHANGE_REASON reason, const CONTEXT *from, CONTEXT *to, INT32 info, VOID *v)
{
    if (reason == CONTEXT_CHANGE_REASON_SIGNAL || reason == CONTEXT_CHANGE_REASON_FATALSIGNAL)
        cleanBridge();
}

void InitBufferManagerConsumer()
{
    InitBufferManager();

    PIN_AddContextChangeFunction(ConsumerSignalCallback, 0);
}

void DeinitBufferManagerConsumer()
{
    DeinitBufferManager();
}

void AllocateThreadConsumer(pid_t tid, int buffer_capacity)
{
    // Start with one page read buffer
    readBufferSize_[tid] = 4096;
    readBuffer_[tid] = malloc(4096);
    assert(readBuffer_[tid]);

    consumeBuffer_[tid] = new Buffer(buffer_capacity);
}

handshake_container_t* front(pid_t tid, bool isLocal)
{

  assert(consumeBuffer_[tid] != NULL);
  if(consumeBuffer_[tid]->size() > 0) {
    handshake_container_t* returnVal = consumeBuffer_[tid]->front();
    return returnVal;
  }

  lk_lock(&locks_[tid], tid+1);

  assert(queueSizes_[tid] > 0);

  while (fileEntryCount_[tid] == 0) {
    lk_unlock(&locks_[tid]);
    while(*consumers_sleep) {
      PIN_SemaphoreWait(consumer_sleep_lock);
      //PIN_Sleep(250);
    }
    PIN_Yield();
    lk_lock(&locks_[tid], tid+1);
  }

  if(fileEntryCount_[tid] > 0) {
    copyFileToConsumer(tid);
    assert(!consumeBuffer_[tid]->empty());
    handshake_container_t* returnVal = consumeBuffer_[tid]->front();
    lk_unlock(&locks_[tid]);
    return returnVal;
  }
  assert(fileEntryCount_[tid] == 0);
  assert(consumeBuffer_[tid]->empty());

  int spins = 0;
  while(consumeBuffer_[tid]->empty()) {
    lk_unlock(&locks_[tid]);
    PIN_Yield();
    while(*consumers_sleep) {
      PIN_SemaphoreWait(consumer_sleep_lock);
    }
    lk_lock(&locks_[tid], tid+1);
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
  lk_unlock(&locks_[tid]);
  return resultVal;
}

int getConsumerSize(pid_t tid)
{
  // Another thread might be doing the allocation
  while (consumeBuffer_[tid] == NULL) ;

  assert(consumeBuffer_[tid] != NULL);
  return consumeBuffer_[tid]->size();
}

/* On the consumer side, signal that we have consumed
 * numChanged buffers that can go back to the core's pool.
 */
void applyConsumerChanges(pid_t tid, int numChanged)
{
  lk_lock(&locks_[tid], tid+1);

  pool_[tid] += numChanged;

  assert(queueSizes_[tid] >= numChanged);
  queueSizes_[tid] -= numChanged;

  lk_unlock(&locks_[tid]);
}

void pop(pid_t tid)
{
  consumeBuffer_[tid]->pop();
  *popped_ = true;
  return;
  lk_lock(&locks_[tid], tid+1);

  assert(consumeBuffer_[tid]->size() > 0);
  consumeBuffer_[tid]->pop();

  pool_[tid] += 1;
  assert(queueSizes_[tid] > 0);
  queueSizes_[tid] -= 1;

  lk_unlock(&locks_[tid]);
}

static void copyFileToConsumer(pid_t tid)
{
  if(useRealFile_) {
    copyFileToConsumerReal(tid);
  }
  else {
    copyFileToConsumerFake(tid);
  }
}

static void copyFileToConsumerFake(pid_t tid)
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

static void copyFileToConsumerReal(pid_t tid)
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

static bool readHandshake(pid_t tid, int fd, handshake_container_t* handshake)
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
