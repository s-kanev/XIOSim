#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <stack>
#include <sstream>
#include <map>
#include <queue>

#include "feeder.h"
#include "Buffer.h"
#include "BufferManager.h"

extern int num_threads;
extern bool consumers_sleep;
extern PIN_SEMAPHORE consumer_sleep_lock;

ostream& operator<< (ostream &out, handshake_container_t &hand)
{
  out << "hand:" << " ";
  out << hand.flags.valid;
  out << hand.flags.isFirstInsn;
  out << hand.flags.isLastInsn;
  out << hand.flags.killThread;
  out << " ";
  out << hand.mem_buffer.size();
  out << " ";
  out << "pc:" << hand.handshake.pc << " ";
  out << "npc:" << hand.handshake.npc << " ";
  out << "tpc:" << hand.handshake.tpc << " ";
  out << "brtaken:" << hand.handshake.brtaken << " ";
  out << "ins:" << hand.handshake.ins << " ";
  out << "flags:" << hand.handshake.sleep_thread << hand.handshake.resume_thread;
  out  << hand.handshake.real;
  out << hand.handshake.in_critical_section;
  out << " slicenum:" << hand.handshake.slice_num << " ";
  out << "feederslicelen:" << hand.handshake.feeder_slice_length << " ";
  out << "feedersliceweight:" << hand.handshake.slice_weight_times_1000 << " ";
  out.flush();
  return out;
}

BufferManager::BufferManager()
  :numThreads_(0)
{
  int pid = getpgrp();
  ostringstream iss;
  iss << pid;
  gpid_ = iss.str();
  assert(gpid_.length() > 0);
  popped_ = false;

  bridgeDirs_.push_back("/dev/shm/");
  bridgeDirs_.push_back("/tmp/");
  bridgeDirs_.push_back("./");
}

BufferManager::~BufferManager()
{
  for(int i = 0; i < (int)bridgeDirs_.size(); i++) {
    string cmd = "/bin/rm -rf " + bridgeDirs_[i] + gpid_ + "_* &";
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
    while(consumers_sleep) {
      PIN_SemaphoreWait(&consumer_sleep_lock);
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
    while(consumers_sleep) {
      PIN_SemaphoreWait(&consumer_sleep_lock);
      //      PIN_Sleep(250);
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

  if(produceBuffer_[tid]->full()) {// || ( (consumeBuffer_[tid]->size() == 0) && (fileEntryCount_[tid] == 0))) {
#ifdef DEBUG
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

unsigned int BufferManager::size(THREADID tid)
{
  lk_lock(locks_[tid], tid+1);
  unsigned int result = queueSizes_[tid];
  lk_unlock(locks_[tid]);
  return result;
}


/* On the producer side, if we have filled up the in-memory
 * buffer, wait until some of it gets consumed. If not,
 * try and increase the backing file size.
 */
void BufferManager::reserveHandshake(THREADID tid)
{
  int queueLimit;
  if(num_threads > 1) {
    queueLimit = 100000001;
  }
  else {
    queueLimit = 100000001;
  }

  if(pool_[tid] > 0) {
    return;
  }

  //  while(pool_[tid] == 0) {
  while(true) {
    assert(queueSizes_[tid] > 0);
    lk_unlock(locks_[tid]);

    enable_consumers();
    disable_producers();

    PIN_Sleep(1000);

    lk_lock(locks_[tid], tid+1);

    if(popped_) {
      popped_ = false;
      continue;
    }

    disable_consumers();
    enable_producers();

    //    if(num_threads == 1 || (!useRealFile_)) {
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
    this->abort();
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
      if(space > 2000000) { // 2 GB
        fileNames_[tid].push_back(genFileName(bridgeDirs_[i]));
        madeFile = true;
        break;
      }
      //cerr << "Out of space on " + bridgeDirs_[i] + " !!!" << endl;
    }
    if(madeFile == false) {
      cerr << "Nowhere left for the poor file bridge :(" << endl;
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
  if(fileNames_[tid].back().find("shm") == string::npos) {
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

  int bytesWritten = write(fd, writeBuffer, totalBytes);
  if(bytesWritten == -1) {
    cerr << "Pipe write error: " << bytesWritten << " Errcode:" << strerror(errno) << endl;
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
  map<THREADID, string>::iterator it;

  for(int i = 0; i < (int)bridgeDirs_.size(); i++) {
    string cmd = "/bin/rm -rf " + bridgeDirs_[i] + gpid_ + "_* &";
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
  /*  if(num_threads > 1) {
    useRealFile_ = true;
    }*/
  //  else {
  useRealFile_ = true;
    //  }

  int bufferEntries = 640000 / 2;
  int bufferCapacity = bufferEntries / 2 / num_threads;

  if(!useRealFile_) {
    bufferCapacity /= 8;
    fakeFile_[tid] = new Buffer(120000);
  }

  consumeBuffer_[tid] = new Buffer(bufferCapacity);
  produceBuffer_[tid] = new Buffer(bufferCapacity);
  assert(produceBuffer_[tid]->capacity() <= consumeBuffer_[tid]->capacity());
  resetPool(tid);
  produceBuffer_[tid]->get_buffer()->flags.isFirstInsn = true;
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

string BufferManager::genFileName(string path)
{
  char* temp = tempnam(path.c_str(), gpid_.c_str());
  string res = string(temp);
  assert(res.find(path) != string::npos);
  res.insert(path.length() + gpid_.length(), "_");
  res = res + ".helix";
  free(temp);
  return res;
}

void BufferManager::resetPool(THREADID tid)
{
  int poolFactor = 1;
  if(num_threads > 1) {
    poolFactor = 6;
  }
  pool_[tid] = (consumeBuffer_[tid]->capacity() + produceBuffer_[tid]->capacity()) * poolFactor;
  //  pool_[tid] = 2000000000;
}

int BufferManager::getKBFreeSpace(string path)
{
  struct statvfs fsinfo;
  statvfs(path.c_str(), &fsinfo);
  return (fsinfo.f_bsize * fsinfo.f_bfree / 1024);
}
