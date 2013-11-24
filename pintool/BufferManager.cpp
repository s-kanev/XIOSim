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
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stack>
#include <sstream>
#include <map>
#include <queue>

#include "multiprocess_shared.h"
#include "../buffer.h"
#include "BufferManager.h"

#include "feeder.h"

BufferManager::BufferManager()
{
  int pid = getpgrp();
  ostringstream iss;
  iss << pid;
  gpid_ = iss.str().c_str();
  assert(gpid_.length() > 0);
  popped_ = false;

  bridgeDirs_.push_back("/dev/shm/");
  bridgeDirs_.push_back("/tmp/");
  bridgeDirs_.push_back("./");

  // This constructor accepts a buckets parameter which negates the need to call
  // reserve on all the maps later.
  queueSizes_.initialize_late(xiosim::shared::XIOSIM_SHARED_MEMORY_KEY,
      xiosim::shared::BUFFER_MANAGER_QUEUE_SIZES_, 16);
  std::cout << "Initialized queueSizes" << std::endl;

  // Reserve space in all maps for 16 cores
  // This reduces the incidence of an annoying race, see
  // comment in empty()
}

BufferManager::~BufferManager()
{
  for(int i = 0; i < (int)bridgeDirs_.size(); i++) {
    boost::interprocess::string cmd = "/bin/rm -rf " + bridgeDirs_[i] + gpid_ + "_* &";
    int retVal = system(cmd.c_str());
    (void)retVal;
    assert(retVal == 0);
  }
}

handshake_container_t* BufferManager::front(THREADID tid, bool isLocal)
{

  if(consumeBuffer_[tid]->size() > 0) {
    handshake_container_t* returnVal = consumeBuffer_[tid]->front();
    return returnVal;
  }

  lk_lock(locks_[tid], tid+1);

  assert(queueSizes_[tid] > 0);

  while (fileEntryCount_[tid] == 0 && produceBuffer_[tid]->size() == 0) {
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

handshake_container_t* BufferManager::back(THREADID tid)
{
  lk_lock(locks_[tid], tid+1);
  assert(queueSizes_[tid] > 0);
  handshake_container_t* returnVal = produceBuffer_[tid]->back();
  lk_unlock(locks_[tid]);
  return returnVal;
}

bool BufferManager::empty(THREADID tid)
{
  // There is a race for initializing the BufferManager maps,
  // where sometimes a new thread is calling allocateThread(),
  // and an already existing thread is calling empty().
  // This hack seems to prevent it, there's probably a much
  // better way to do this though...
  if(!locks_[tid]) {
    PIN_Sleep(1000);
  }
  assert(locks_[tid]);

  lk_lock(locks_[tid], tid+1);
  bool result = queueSizes_[tid] == 0;
  lk_unlock(locks_[tid]);
  return result;
}

/* On the producer side, get a buffer which we can start
 * filling directly.
 */

handshake_container_t* BufferManager::get_buffer(THREADID tid)
{
  lk_lock(locks_[tid], tid+1);
  // Push is guaranteed to succeed because each call to
  // this->get_buffer() is followed by a call to this->producer_done()
  // which will make space if full
  handshake_container_t* result = produceBuffer_[tid]->get_buffer();
  produceBuffer_[tid]->push_done();
  queueSizes_[tid]++;
  assert(pool_[tid] > 0);
  pool_[tid]--;

  lk_unlock(locks_[tid]);
  return result;
}

/* On the producer side, signal that we are done filling the
 * current buffer. If we have ran out of space, make space
 * for a new buffer, so get_buffer() cannot fail.
 */
void BufferManager::producer_done(THREADID tid, bool keepLock)
{
  lk_lock(locks_[tid], tid+1);

  ASSERTX(!produceBuffer_[tid]->empty());
  handshake_container_t* last = produceBuffer_[tid]->back();
  ASSERTX(last->flags.valid);

  if(!keepLock) {
    reserveHandshake(tid);
  }
  else {
    pool_[tid]++; // Expand in case the last handshakes need space
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

  lk_unlock(locks_[tid]);
}

/* On the producer side, flush all buffers associated
 * with a thread to the backing file.
 */
void BufferManager::flushBuffers(THREADID tid)
{
  lk_lock(locks_[tid], tid+1);

  /* If you call this from a consumer thread, you're going to have
   * a bad (race-y) time! */
  THREADID caller_tid = PIN_ThreadId();
  assert(this->hasThread(caller_tid));

  if(produceBuffer_[tid]->size() > 0) {
    copyProducerToFile(tid, false);
  }
  lk_unlock(locks_[tid]);
}

int BufferManager::getConsumerSize(THREADID tid)
{
  return consumeBuffer_[tid]->size();
}

/* On the consumer side, signal that we have consumed
 * numChanged buffers that can go back to the core's pool.
 */
void BufferManager::applyConsumerChanges(THREADID tid, int numChanged)
{
  lk_lock(locks_[tid], tid+1);

  pool_[tid] += numChanged;

  assert(queueSizes_[tid] >= numChanged);
  queueSizes_[tid] -= numChanged;

  lk_unlock(locks_[tid]);
}

void BufferManager::pop(THREADID tid)
{
  consumeBuffer_[tid]->pop();
  popped_ = true;
  return;
  lk_lock(locks_[tid], tid+1);

  assert(consumeBuffer_[tid]->size() > 0);
  consumeBuffer_[tid]->pop();

  pool_[tid] += 1;
  assert(queueSizes_[tid] > 0);
  queueSizes_[tid] -= 1;

  lk_unlock(locks_[tid]);
}

bool BufferManager::hasThread(THREADID tid)
{
  bool result = queueSizes_.count(tid);
  return (result != 0);
}

uint64_t BufferManager::size(THREADID tid)
{
  lk_lock(locks_[tid], tid+1);
  uint64_t result = queueSizes_[tid];
  lk_unlock(locks_[tid]);
  return result;
}


/* On the producer side, if we have filled up the in-memory
 * buffer, wait until some of it gets consumed. If not,
 * try and increase the backing file size.
 */
void BufferManager::reserveHandshake(THREADID tid)
{
  if(pool_[tid] > 0) {
    return;
  }

  //  while(pool_[tid] == 0) {
  while(true) {
    assert(queueSizes_[tid] > 0);
    lk_unlock(locks_[tid]);

    enable_consumers();
    disable_producers();

    PIN_Sleep(500);

    lk_lock(locks_[tid], tid+1);

    if(popped_) {
      popped_ = false;
      continue;
    }

    disable_consumers();
    enable_producers();

    if(pool_[tid] > 0) {
      break;
    }

    if(num_cores == 1) {
      continue;
    }

    pool_[tid] += 50000;

#ifdef ZESTO_PIN_DBG
    cerr << tid << " [reserveHandshake()]: Increasing file up to " << queueSizes_[tid] + pool_[tid] << endl;
#endif
    break;
  }
}

void BufferManager::copyProducerToFile(THREADID tid, bool checkSpace)
{
  if(useRealFile_) {
    copyProducerToFileReal(tid, checkSpace);
  }
  else {
    copyProducerToFileFake(tid);
  }
}

void BufferManager::copyFileToConsumer(THREADID tid)
{
  if(useRealFile_) {
    copyFileToConsumerReal(tid);
  }
  else {
    copyFileToConsumerFake(tid);
  }
}

void BufferManager::copyProducerToFileFake(THREADID tid)
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


void BufferManager::copyFileToConsumerFake(THREADID tid)
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


void BufferManager::copyProducerToFileReal(THREADID tid, bool checkSpace)
{
  int result;
  bool madeFile = false;
  if(checkSpace) {
    for(int i = 0; i < (int)bridgeDirs_.size(); i++) {
      int space = getKBFreeSpace(bridgeDirs_[i]);
      if(space > 2500000) { // 2.5 GB
        fileNames_[tid].push_back(genFileName(bridgeDirs_[i]));
        madeFile = true;
        break;
      }
      //cerr << "Out of space on " + bridgeDirs_[i] + " !!!" << endl;
    }
    if(madeFile == false) {
      cerr << "Nowhere left for the poor file bridge :(" << endl;
      cerr << "BridgeDirs:" << endl;
      for(int i = 0; i < (int)bridgeDirs_.size(); i++) {
        int space = getKBFreeSpace(bridgeDirs_[i]);
        cerr << bridgeDirs_[i] << ":" << space << " in KB" << endl;
      }
      this->abort();
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
    this->abort();
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
    this->abort();
  }

  // sync() if we put the file somewhere besides /dev/shm
  if(fileNames_[tid].back().find("shm") == boost::interprocess::string::npos) {
    sync();
  }

  assert(produceBuffer_[tid]->size() == 0);
  assert(fileEntryCount_[tid] >= 0);
}


void BufferManager::copyFileToConsumerReal(THREADID tid)
{
  int result;

  int fd = open(fileNames_[tid].front().c_str(), O_RDWR);
  if(fd == -1) {
    cerr << "Opened to read: " << fileNames_[tid].front();
    cerr << "Pipe open error: " << fd << " Errcode:" << strerror(errno) << endl;
    this->abort();
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
    this->abort();
  }

  assert(fileCounts_[tid].front() == 0);
  result = remove(fileNames_[tid].front().c_str());
  if(result != 0) {
    cerr << "Remove error: " << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }

  fileNames_[tid].pop_front();
  fileCounts_[tid].pop_front();

  assert(fileEntryCount_[tid] >= 0);
}

static ssize_t do_write(const int fd, const void* buff, const size_t size)
{
  ssize_t bytesWritten = 0;
  do {
    ssize_t res = write(fd, (void*)((char*)buff + bytesWritten), size - bytesWritten);
    if(res == -1) {
      cerr << "failed write!" << endl;
      cerr << "bytesWritten:" << bytesWritten << endl;
      cerr << "size:" << size << endl;
      return -1;
    }
    bytesWritten += res;
  } while (bytesWritten < (ssize_t)size);
  return bytesWritten;
}

void  BufferManager::writeHandshake(THREADID tid, int fd, handshake_container_t* handshake)
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

  map<UINT32, UINT8>::iterator it;
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

    cerr << "Opened to write: " << fileNames_[tid].back() << endl;
    cerr << "Thread Id:" << tid << endl;
    cerr << "fd:" << fd << endl;
    cerr << "Queue Size:" << queueSizes_[tid] << endl;
    cerr << "ConsumeBuffer size:" << consumeBuffer_[tid]->size() << endl;
    cerr << "ProduceBuffer size:" << produceBuffer_[tid]->size() << endl;
    cerr << "file entry count:" << fileEntryCount_[tid] << endl;

    cerr << "BridgeDirs:" << endl;
    for(int i = 0; i < (int)bridgeDirs_.size(); i++) {
      int space = getKBFreeSpace(bridgeDirs_[i]);
      cerr << bridgeDirs_[i] << ":" << space << " in KB" << endl;
    }
    this->abort();
  }
  if(bytesWritten != totalBytes) {
    cerr << "File write error: " << bytesWritten << " expected:" << totalBytes << endl;
    cerr << fileNames_[tid].back() << endl;
    this->abort();
  }
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

bool BufferManager::readHandshake(THREADID tid, int fd, handshake_container_t* handshake)
{
  const int handshakeBytes = sizeof(P2Z_HANDSHAKE);
  const int flagBytes = sizeof(handshake_flags_t);
  const int mapEntryBytes = sizeof(UINT32) + sizeof(UINT8);

  int mapSize;
  int bytesRead = do_read(fd, &(mapSize), sizeof(int));
  if(bytesRead == -1) {
    cerr << "File read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }
  assert(bytesRead == sizeof(int));

  int mapBytes = mapSize * mapEntryBytes;
  int totalBytes = handshakeBytes + flagBytes + mapBytes;

  void * readBuffer = readBuffer_[tid];
  assert(readBuffer != NULL);

  bytesRead = do_read(fd, readBuffer, totalBytes);
  if(bytesRead == -1) {
    cerr << "File read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
    this->abort();
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

void BufferManager::abort(){
  this->signalCallback(SIGABRT);
  exit(1);
  abort();
}

void BufferManager::signalCallback(int signum)
{
  cerr << "BufferManager caught signal:" << signum << endl;
  map<THREADID, boost::interprocess::string>::iterator it;

  for(int i = 0; i < (int)bridgeDirs_.size(); i++) {
    boost::interprocess::string cmd = "/bin/rm -rf " + bridgeDirs_[i] + gpid_ + "_* &";
    int retVal = system(cmd.c_str());
    (void)retVal;
    assert(retVal == 0);
  }
}

void BufferManager::allocateThread(THREADID tid)
{
  assert(queueSizes_.count(tid) == 0);

  queueSizes_[tid] = 0;
  numThreads_++;
  fileEntryCount_[tid] = 0;
  /*  if(num_cores > 1) {
    useRealFile_ = true;
    }*/
  //  else {
  useRealFile_ = true;
    //  }

  int bufferEntries = 640000 / 2;
  int bufferCapacity = bufferEntries / 2 / KnobNumCores.Value();

  if(!useRealFile_) {
    bufferCapacity /= 8;
    fakeFile_[tid] = new Buffer(120000);
  }

  consumeBuffer_[tid] = new Buffer(bufferCapacity);
  produceBuffer_[tid] = new Buffer(bufferCapacity);
  assert(produceBuffer_[tid]->capacity() <= consumeBuffer_[tid]->capacity());
  resetPool(tid);
  locks_[tid] = new XIOSIM_LOCK();
  lk_init(locks_[tid]);

  cerr << tid << " Creating temp files with prefix "  << gpid_ << "_*" << endl;
  // Start with one page read buffer
  readBufferSize_[tid] = 4096;
  readBuffer_[tid] = malloc(4096);
  assert(readBuffer_[tid]);

  writeBufferSize_[tid] = 4096;
  writeBuffer_[tid] = malloc(4096);
  assert(writeBuffer_[tid]);
}

boost::interprocess::string BufferManager::genFileName(boost::interprocess::string path)
{
  char* temp = tempnam(path.c_str(), gpid_.c_str());
  boost::interprocess::string res = boost::interprocess::string(temp);
  assert(res.find(path) != boost::interprocess::string::npos);
  res.insert(path.length() + gpid_.length(), "_");
  res = res + ".helix";
  free(temp);
  return res;
}

void BufferManager::resetPool(THREADID tid)
{
  int poolFactor = 3;
  assert(poolFactor >= 1);
  pool_[tid] = (consumeBuffer_[tid]->capacity() + produceBuffer_[tid]->capacity()) * poolFactor;
  //  pool_[tid] = 2000000000;
}

int BufferManager::getKBFreeSpace(boost::interprocess::string path)
{
  struct statvfs fsinfo;
  statvfs(path.c_str(), &fsinfo);
  return ((unsigned long long)fsinfo.f_bsize * (unsigned long long)fsinfo.f_bavail / 1024);
}
