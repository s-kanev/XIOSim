#include <unordered_map>

#include "boost_interprocess.h"

#include "../interface.h"
#include "multiprocess_shared.h"
#include "BufferManagerConsumer.h"

using namespace std;

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
static std::unordered_map<pid_t, regs_t*> shadowRegs_;

void InitBufferManagerConsumer(pid_t harness_pid, int num_cores)
{
    InitBufferManager(harness_pid, num_cores);
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
    shadowRegs_[tid] = (regs_t*)calloc(1, sizeof(regs_t));

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
    yield();
    wait_consumers();
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
    yield();
    wait_consumers();
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
  if (numChanged == 0)
    return;

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
  int bufferSize;
  int bytesRead = do_read(fd, &(bufferSize), sizeof(int));
  if(bytesRead == -1) {
    cerr << "File read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  assert(bytesRead == sizeof(int));

  /* We have read the size field already. Now read the rest. */
  bufferSize -= sizeof(bufferSize);

  void * readBuffer = readBuffer_[tid];
  assert(readBuffer != NULL);

  bytesRead = do_read(fd, readBuffer, bufferSize);
  if(bytesRead == -1) {
    cerr << "File read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  assert(bytesRead == bufferSize);

  /* Prepare handshake regs from shadow regs. So we can apply delta compression. */
  regs_t * shadow_regs = shadowRegs_[tid];
  memcpy(&(handshake->handshake.ctxt), shadow_regs, sizeof(regs_t));

  handshake->Deserialize(readBuffer, bufferSize);

  /* This is ugly and maybe costly. Update the shadow copy.
   * If we care enough, we should double-buffer */
  memcpy(shadow_regs, &(handshake->handshake.ctxt), sizeof(regs_t));

  return true;
}

}
}
