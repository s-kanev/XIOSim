#include "BufferManager.h"

#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stack>

extern int num_threads;

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
  out << "coreid:" << hand.handshake.coreID << " ";
  out << "npc:" << hand.handshake.npc << " ";
  out << "tpc:" << hand.handshake.tpc << " ";
  out << "brtaken:" << hand.handshake.brtaken << " ";
  out << "ins:" << hand.handshake.ins << " ";
  out << "flags:" << hand.handshake.sleep_thread << hand.handshake.resume_thread;
  out << hand.handshake.iteration_correction << hand.handshake.real;
  out << hand.handshake.in_critical_section;
  out << " slicenum:" << hand.handshake.slice_num << " ";
  out << "feederslicelen:" << hand.handshake.feeder_slice_length << " ";
  out << "feedersliceweight:" << hand.handshake.slice_weight_times_1000 << " ";
  out.flush();
  return out;
}

BufferManager::BufferManager()
{
  useRealFile_ = true;
}

BufferManager::~BufferManager()
{
  map<THREADID, string>::iterator it;
  for(it = fileNames_.begin(); it != fileNames_.end(); it++) {
    string cmd = "/bin/rm -rf " + fileNames_[it->first] + " " + bogusNames_[it->first];
    assert(system(cmd.c_str()) == 0);
  }
}

handshake_container_t* BufferManager::front(THREADID tid, bool isLocal)
{
  if(consumeBuffer_[tid]->size() > 0) {
    assert(isLocal);
    handshake_container_t* returnVal = consumeBuffer_[tid]->front();
    return returnVal;
  }

  assert(!isLocal);

  GetLock(locks_[tid], tid+1);

  assert(queueSizes_[tid] > 0);

  assert(fileEntryCount_[tid] > 0 || produceBuffer_[tid]->size() > 0);

  if(fileEntryCount_[tid] > 0) {
    copyFileToConsumer(tid);
    assert(!consumeBuffer_[tid]->empty());
    handshake_container_t* returnVal = consumeBuffer_[tid]->front();
    ReleaseLock(locks_[tid]);
    return returnVal;
  }
  assert(fileEntryCount_[tid] == 0);
  assert(consumeBuffer_[tid]->empty());

  int spins = 0;
  while(consumeBuffer_[tid]->empty()) {
    ReleaseLock(locks_[tid]);
    ReleaseLock(&simbuffer_lock);
    PIN_Yield();    
    GetLock(&simbuffer_lock, tid+1);
    GetLock(locks_[tid], tid+1);
    spins++;
    if(spins >= 2) { // DONT CHANGE THIS MUST EQUAL 3
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
  ReleaseLock(locks_[tid]);
  return resultVal;
}

handshake_container_t* BufferManager::back(THREADID tid)
{
  GetLock(locks_[tid], tid+1);
  assert(queueSizes_[tid] > 0);
  handshake_container_t* returnVal = produceBuffer_[tid]->back();
  ReleaseLock(locks_[tid]);
  return returnVal;
}

bool BufferManager::empty(THREADID tid)
{
  GetLock(locks_[tid], tid+1);
  bool result = queueSizes_[tid] == 0;
  if(result) {
    pool_[tid] = consumeBuffer_[tid]->capacity() + (produceBuffer_[tid]->capacity() * 2);
  }
  ReleaseLock(locks_[tid]);
  return result;
}

handshake_container_t* BufferManager::get_buffer(THREADID tid)
{
  GetLock(locks_[tid], tid+1);
  // Push is guaranteed to succeed because each call to
  // this->get_buffer() is followed by a call to this->producer_done()
  // which will make space if full
  handshake_container_t* result = produceBuffer_[tid]->get_buffer();
  produceBuffer_[tid]->push_done();
  queueSizes_[tid]++;
  assert(pool_[tid] > 0);
  pool_[tid]--;

  ReleaseLock(locks_[tid]);
  return result;
}

void BufferManager::producer_done(THREADID tid, bool keepLock)
{
  GetLock(locks_[tid], tid+1);

  ASSERTX(!produceBuffer_[tid]->empty());
  handshake_container_t* last = produceBuffer_[tid]->back();
  ASSERTX(last->flags.valid);
  
  if(!keepLock) {
    reserveHandshake(tid);
    ReleaseLock(&simbuffer_lock);
  }
  else {
    pool_[tid]++;
  }

  if(produceBuffer_[tid]->full() || ( (consumeBuffer_[tid]->size() == 0) && (fileEntryCount_[tid] == 0))) {    
#ifdef DEBUG  
    int produceSize = produceBuffer_[tid]->size();
#endif
    copyProducerToFile(tid);
    assert(fileEntryCount_[tid] > 0);
    assert(fileEntryCount_[tid] >= produceSize);
    assert(produceBuffer_[tid]->size() == 0);
  }
  
  assert(!produceBuffer_[tid]->full());
  
  ReleaseLock(locks_[tid]);

  
  if(!keepLock) {
    GetLock(&simbuffer_lock, tid+1);
  }
}

void BufferManager::flushBuffers(THREADID tid)
{
  GetLock(locks_[tid], tid+1);
  map<THREADID, string>::iterator it;
  cerr << "FLUSHWRITE:" << tid << endl;

  if(produceBuffer_[tid]->size() > 0) {
    cerr << produceBuffer_[tid]->size() << " " << fileEntryCount_[tid] << " " << consumeBuffer_[tid]->size() << endl;
    copyProducerToFile(tid);
    cerr << produceBuffer_[tid]->size() << " " << fileEntryCount_[tid] << " " << consumeBuffer_[tid]->size() << endl;
    cerr << "Done copy" << tid << endl;
  }
  else {
    cerr << "Skipped copy" << tid << endl;
  }

  ReleaseLock(locks_[tid]);
}

int BufferManager::getConsumerSize(THREADID tid)
{
  return consumeBuffer_[tid]->size();
}

void BufferManager::applyConsumerChanges(THREADID tid, int numChanged)
{
  GetLock(locks_[tid], tid+1);
  
  pool_[tid] += numChanged;

  assert(queueSizes_[tid] >= numChanged);
  queueSizes_[tid] -= numChanged;

  ReleaseLock(locks_[tid]);
}

void BufferManager::pop(THREADID tid)
{
  popped_ = true;
  assert(consumeBuffer_[tid]->size() > 0);
  consumeBuffer_[tid]->pop();
}

bool BufferManager::hasThread(THREADID tid)
{
  bool result = queueSizes_.count(tid);
  return (result != 0);
}

unsigned int BufferManager::size()
{
  unsigned int result = queueSizes_.size();
  return result;
}

void BufferManager::reserveHandshake(THREADID tid)
{
  long long int spins = 0;
  bool popped_ = false;

  while(pool_[tid] == 0) {
    ReleaseLock(locks_[tid]);
    ReleaseLock(&simbuffer_lock);
    PIN_Yield();    
    GetLock(&simbuffer_lock, tid+1);
    GetLock(locks_[tid], tid+1);
    spins++;

    if(spins >= 7000000LL) {
      assert(queueSizes_[tid] > 0);
      if(queueSizes_[tid] < 10000001) {
	pool_[tid] += 50000;
	cerr << tid << " [reserveHandshake()]: Increasing file up to " << queueSizes_[tid] + pool_[tid] << endl;
	spins = 0;
	break;
      }
      else {
	cerr << tid << " [reserveHandshake()]: File size too big to expand, abort():" << queueSizes_[tid] << endl;
	this->abort();
      }
    }
    if(popped_) {
      popped_ = false;
      spins = 0;
    }
  }
}

void BufferManager::copyProducerToFile(THREADID tid)
{
  if(useRealFile_) {
    copyProducerToFileReal(tid);
  }
  else {
    assert(false);
    copyProducerToFileFake(tid);
  }
  sync();
}

void BufferManager::copyFileToConsumer(THREADID tid)
{
  if(useRealFile_) {
    copyFileToConsumerReal(tid);
  }
  else {
    assert(false);
    copyFileToConsumerFake(tid);
  }
  sync();
}

void BufferManager::copyProducerToFileFake(THREADID tid)
{
  while(produceBuffer_[tid]->size() > 0) {
    if(fakeFile_[tid]->full()) {
      break;
    }

    if(produceBuffer_[tid]->front()->flags.valid == false) {
      break;
    }

    handshake_container_t* handshake = fakeFile_[tid]->get_buffer();
    produceBuffer_[tid]->front()->CopyTo(handshake);
    fakeFile_[tid]->push_done();
    produceBuffer_[tid]->pop();
  }
}


void BufferManager::copyFileToConsumerFake(THREADID tid)
{
  while(fakeFile_[tid]->size() > 0) {
    if(consumeBuffer_[tid]->full()) {
      break;
    }

    assert(fakeFile_[tid]->front()->flags.valid);

    handshake_container_t* handshake = consumeBuffer_[tid]->get_buffer();
    fakeFile_[tid]->front()->CopyTo(handshake);
    consumeBuffer_[tid]->push_done();
    fakeFile_[tid]->pop();
  }
}


void BufferManager::copyProducerToFileReal(THREADID tid)
{
  int result;

  int fd_bogus = open(bogusNames_[tid].c_str(), O_WRONLY | O_CREAT, 0777);
  if(fd_bogus == -1) {
    cerr << "Opened to write: " << bogusNames_[tid].c_str();
    cerr << "Pipe open error: " << fd_bogus << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }

  for(int i = 0; i < produceBuffer_[tid]->size(); i++) {
    assert(produceBuffer_[tid]->getElement(i)->flags.valid);
    writeHandshake(tid, fd_bogus, produceBuffer_[tid]->getElement(i));
    fileEntryCount_[tid]++;
  }
  
  while(produceBuffer_[tid]->size() > 0) {
    produceBuffer_[tid]->pop();
  }

  sync();

  int fd = open(fileNames_[tid].c_str(), O_RDONLY);
  if(fd == -1) {
    cerr << "Opened to write: " << fileNames_[tid].c_str();
    cerr << "Pipe open error: " << fd << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }

  const int sizeBuf = 1024 * 1024;
  uint8_t buf[sizeBuf];
  memset(buf, 0, sizeBuf); 

  int bytesRead;
  do {
    bytesRead = read(fd, buf, sizeBuf);
    if(bytesRead == -1) {
      cerr << "Read ptof error: " << " Errcode:" << strerror(errno) << endl;
      this->abort();
    }
    if(bytesRead > 0) {
      int bytesWritten = 0, writesAttempt = 0;
      uint8_t * buf_curr = buf;
      while (bytesWritten < bytesRead) {
        bytesWritten += write(fd_bogus, buf_curr, bytesRead);
        buf_curr += bytesWritten;
        writesAttempt++;
        if (writesAttempt > 1) {
          cerr << "Write ptof error: " << " Errcode:" << strerror(errno) << endl;
          this->abort();
        }
      }
    }
  } while(bytesRead > 0);

  result = close(fd);
  if(result == -1) {
    cerr << "Close error: " << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }

  result = close(fd_bogus);
  if(result == -1) {
    cerr << "Close error: " << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }

  sync();

  result = rename(bogusNames_[tid].c_str(), fileNames_[tid].c_str());
  if(result == -1) {
    cerr << "Can't rename filesystem bridge files. " << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }
  sync();

  assert(produceBuffer_[tid]->size() == 0);
  assert(fileEntryCount_[tid] >= 0);
}


void BufferManager::copyFileToConsumerReal(THREADID tid)
{
  int result;

  int fd = open(fileNames_[tid].c_str(), O_RDWR);
  if(fd == -1) {
    cerr << "Opened to read: " << fileNames_[tid].c_str();
    cerr << "Pipe open error: " << fd << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }

  int count = 0;
  bool validRead = true;
  while(fileEntryCount_[tid] > 0) {
    if(consumeBuffer_[tid]->full()) {
      break;
    }
    handshake_container_t* handshake = consumeBuffer_[tid]->get_buffer();
    validRead = readHandshake(tid, fd, handshake);
    assert(validRead);
    consumeBuffer_[tid]->push_done();
    count++;
    fileEntryCount_[tid]--;
  }

  result = close(fd);
  if(result == -1) {
    cerr << "Close error: " << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }

  sync();
  assert(fileEntryCount_[tid] >= 0);
}

void BufferManager::writeHandshake(THREADID tid, int fd, handshake_container_t* handshake)
{
  int mapSize = handshake->mem_buffer.size();
  const int handshakeBytes = sizeof(P2Z_HANDSHAKE);
  const int flagBytes = sizeof(handshake_flags_t);
  const int mapEntryBytes = sizeof(UINT32) + sizeof(UINT8);
  int mapBytes = mapSize * mapEntryBytes;
  int totalBytes = sizeof(int) + handshakeBytes + flagBytes + mapBytes;

  void * writeBuffer = writeBuffer_[tid];
  void * buffPosition = writeBuffer;

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

  memcpy((char*)buffPosition, &(mapSize), sizeof(int));
  buffPosition = (char*)buffPosition + sizeof(int);

  assert(((unsigned long long int)writeBuffer) + totalBytes == ((unsigned long long int)buffPosition));

  int bytesWritten = write(fd, writeBuffer, totalBytes);
  if(bytesWritten == -1) {
    cerr << "Pipe write error: " << bytesWritten << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }
  if(bytesWritten != totalBytes) {
    cerr << "File write error: " << bytesWritten << " expected:" << totalBytes << endl;
    this->abort();
  }
}

bool BufferManager::readHandshake(THREADID tid, int fd, handshake_container_t* handshake)
{
  const int handshakeBytes = sizeof(P2Z_HANDSHAKE);
  const int flagBytes = sizeof(handshake_flags_t);
  const int mapEntryBytes = sizeof(UINT32) + sizeof(UINT8);

  off_t offset = lseek(fd, 0, SEEK_END);
  assert(offset > 0);
  offset = lseek(fd, offset-(sizeof(int)), SEEK_SET);
  if(offset == -1) {
    cerr << "File seek error: " << " Errcode:" << strerror(errno) << endl;
    this->abort();
  } 
  
  int mapSize;
  int bytesRead = read(fd, &(mapSize), sizeof(int));
  if(bytesRead == -1) {
    cerr << "File read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
    this->abort();
  } 
  assert(bytesRead == sizeof(int));

  int mapBytes = mapSize * mapEntryBytes;
  int totalBytes = handshakeBytes + flagBytes + mapBytes;

  offset = lseek(fd, 0, SEEK_END);
  offset = lseek(fd, offset-(totalBytes + sizeof(int)), SEEK_SET);
  
  assert((offset == 0) || (offset > sizeof(int)));

  void * readBuffer = readBuffer_[tid];
  assert(readBuffer != NULL);

  bytesRead = read(fd, readBuffer, totalBytes);
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

  int trunc_result = ftruncate(fd, offset);
  if(trunc_result != 0) {
    cerr << "File truncate error: " << offset << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }

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
  for(it = fileNames_.begin(); it != fileNames_.end(); it++) {
    string cmd = "/bin/rm -rf " + fileNames_[it->first] + " " + bogusNames_[it->first] + " &";
    assert(system(cmd.c_str()) == 0);
  }
}

void BufferManager::allocateThread(THREADID tid) 
{
  assert(queueSizes_.count(tid) == 0);
  
  queueSizes_[tid] = 0;
  fileEntryCount_[tid] = 0;
  consumeBuffer_[tid] = new Buffer(1000000 / num_threads);
  //    fakeFile_[tid] = new Buffer(2);
  produceBuffer_[tid] = new Buffer(1000);
  produceBuffer_[tid]->get_buffer()->flags.isFirstInsn = true;
  pool_[tid] = consumeBuffer_[tid]->capacity() + (produceBuffer_[tid]->capacity() * 2);
  locks_[tid] = new PIN_LOCK();
  InitLock(locks_[tid]);
  
  /*    char s_tid[100];
	sprintf(s_tid, "%d", tid);
    string logName = "./output_ring_cache/handshake_" + string(s_tid) + ".log";
    logs_[tid] = new ofstream();
    (*(logs_[tid])).open(logName.c_str());*/

  fileNames_[tid] = tempnam("/dev/shm/", "A");
  bogusNames_[tid] = tempnam("/dev/shm/", "B");
  
  cerr << tid << " Created " << fileNames_[tid] << " and " << bogusNames_[tid] << endl;
  
  int fd = open(fileNames_[tid].c_str(), O_WRONLY | O_CREAT, 0777);
  int result = close(fd);
  if(result == -1) {
    cerr << "Close error: " << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }

  // Start with one page read buffer
  readBufferSize_[tid] = 4096;
  readBuffer_[tid] = malloc(4096);
  assert(readBuffer_[tid]);

  writeBufferSize_[tid] = 4096;
  writeBuffer_[tid] = malloc(4096);
  assert(writeBuffer_[tid]);
}
