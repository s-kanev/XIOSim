#include "BufferManager.h"


#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

void BufferManager::signalHandler(int signum) 
{
  cerr << "CAUGHT SIGNAL:" << signum << endl;
  map<THREADID, string>::iterator it;
  for(it = fileNames_.begin(); it != fileNames_.end(); it++) {
    string cmd = "/bin/rm -f " + it->second;
    system(cmd.c_str());
  }

  for(it = bogusNames_.begin(); it != bogusNames_.end(); it++) {
    string cmd = "/bin/rm -f " + it->second;
    system(cmd.c_str());
  }
  exit(1);
}

BufferManager::BufferManager()
{  
}

void BufferManager::threadDone(THREADID tid)
{
  string cmd = "/bin/rm -f " + fileNames_[tid];
  cerr << "Executing " << cmd << endl;
  assert(system(cmd.c_str()) == 0);
  
  cmd = "/bin/rm -f " + bogusNames_[tid];
  cerr << "Executing " << cmd << endl;
  assert(system(cmd.c_str()) == 0);
}

BufferManager::~BufferManager()
{
  cerr << "DESTRUCTING BUFFER MANAGER" << endl;
  map<THREADID, string>::iterator it;

  for(it = fileNames_.begin(); it != fileNames_.end(); it++) {
    string cmd = "/bin/rm -f " + it->second;
    system(cmd.c_str());
  }

  for(it = bogusNames_.begin(); it != bogusNames_.end(); it++) {
    string cmd = "/bin/rm -f " + it->second;
    system(cmd.c_str());
  }
}

handshake_container_t* BufferManager::front(THREADID tid)
{
  checkFirstAccess(tid);
  assert(queueSizes_[tid] > 0);

  if(consumeBuffer_[tid]->size() > 0) {
    return consumeBuffer_[tid]->front();
  }
  
  long long int spins = 0;  
  while(fileEntryCount_[tid] <= 0) {
    ReleaseLock(&simbuffer_lock);
    PIN_Yield();
    GetLock(&simbuffer_lock, tid+1);
    spins++;
    if(spins >= 10000000LL) {
      cerr << "[front()]: That's a lot of spins!" << endl;
      spins = 0;
    }
  }

  readFileIntoConsumeBuffer(tid);
  assert(consumeBuffer_[tid]->size() > 0);
  return consumeBuffer_[tid]->front();

}

handshake_container_t* BufferManager::back(THREADID tid)
{
  checkFirstAccess(tid);
  assert(queueSizes_[tid] > 0);
  assert(produceBuffer_[tid]->size() > 0);
  return(produceBuffer_[tid]->back());
}

bool BufferManager::empty(THREADID tid)
{
  checkFirstAccess(tid);    
  return queueSizes_[tid] == 0;
}

//XXX: Assumes we are holding simbuffer_lock
handshake_container_t* BufferManager::get_buffer(THREADID tid)
{
  checkFirstAccess(tid);

  // Push is guaranteed to succeed because each call to
  // this->get_buffer() is followed by a call to this->producer_done()
  // which will make space if full
  handshake_container_t* result = produceBuffer_[tid]->get_buffer();
  produceBuffer_[tid]->push_done();
  queueSizes_[tid]++;

  return result;
}

//XXX: Assumes we are holding simbuffer_lock
void BufferManager::producer_done(THREADID tid)
{
  ASSERTX(!produceBuffer_[tid]->empty());
  handshake_container_t* last = produceBuffer_[tid]->back();
  ASSERTX(last->flags.valid);

  if(produceBuffer_[tid]->full()) {
    writeProduceBufferIntoFile(tid);

    //    cerr << "slowing it down..." << endl;
    // long long int spins = 0;
    //for(int i = 0; i < queueSizes_[tid] * 10; i++) {
    //  spins++;
    // }
    //cerr << "slowed it down..." << endl;
  }
  
  long long int spins = 0;
  while(fileEntryCount_[tid] >= 250000) {
    ReleaseLock(&simbuffer_lock);
    PIN_Yield();
    GetLock(&simbuffer_lock, tid+1);
    spins++;
    if(spins >= 70000000LL) {
      cerr << tid << "[handshake_buffer.push()]: That's a lot of spins!" << endl;
      spins = 0;
      break;
    }
  }
}

void BufferManager::flushBuffers(THREADID tid)
{
  cerr << "FLUSH_BUFFERS:" << tid << endl;
  map<THREADID, string>::iterator it;
  for(it = fileNames_.begin(); it != fileNames_.end(); it++) {
    cerr << "FLUSHWRITE:" << it->first << endl;
    writeProduceBufferIntoFile(it->first);
  }
  /*  for(it = fileNames_.begin(); it != fileNames_.end(); it++) {
    cerr << "FLUSHREAD1:" << it->first << endl;
    if(fileEntryCount_[tid] > 0) {
      cerr << "FLUSHREAD2:" << fileEntryCount_[tid] << " " << it->first << endl;
      readFileIntoConsumeBuffer(it->first);
    }
    }*/
}

void BufferManager::pop(THREADID tid)
{
  checkFirstAccess(tid);
  
  handshake_container_t* handshake = front(tid);
  (void) handshake;
  
  consumeBuffer_[tid]->pop();
  
  queueSizes_[tid]--;
}

bool BufferManager::hasThread(THREADID tid) 
{
  return (queueSizes_.count(tid) != 0);
}

unsigned int BufferManager::size()
{
  return queueSizes_.size();
}

void BufferManager::checkFirstAccess(THREADID tid)
{
  if(queueSizes_.count(tid) == 0) {
    
    //PIN_Yield();
    //    sleep(30);
    //PIN_Yield();
        
    fileNames_[tid] = tempnam("/dev/shm/", "A");
    bogusNames_[tid] = tempnam("/dev/shm/", "B");

    cerr << "Created " << fileNames_[tid] << " and " << bogusNames_[tid] << endl;

    int fd = open(fileNames_[tid].c_str(), O_WRONLY | O_CREAT, 0777);
    int result = close(fd);
    if(result == -1) {
      cerr << "Close error: " << " Errcode:" << strerror(errno) << endl;  
      abort();
    }
    
    
    queueSizes_[tid] = 0;

    produceBuffer_[tid] = new Buffer();
    produceBuffer_[tid]->get_buffer()->flags.isFirstInsn = true;
    consumeBuffer_[tid] = new Buffer();       
    
    fileEntryCount_[tid] = 0;
    
    locks_[tid] = new PIN_LOCK();
    InitLock(locks_[tid]);
  }
}

void BufferManager::writeProduceBufferIntoFile(THREADID tid)
{
  //  cerr << tid << " Writing produce buffer into file" << endl;
  //  cerr << tid << " File has " << fileEntryCount_[tid] << " entries" << endl;
  GetLock(locks_[tid], tid+1);
  handshake_container_t* handshake;
  int result;

  int fd = open(fileNames_[tid].c_str(), O_WRONLY | O_APPEND);
  if(fd == -1) {
    cerr << "Opened to write: " << fileNames_[tid].c_str();
    cerr << "Pipe open error: " << fd << " Errcode:" << strerror(errno) << endl;
    abort();
  }

  int count = 0;
  while(produceBuffer_[tid]->size() > 0) {
    handshake = produceBuffer_[tid]->front();    
    writeHandshake(fd, handshake);
    produceBuffer_[tid]->pop();
    count++;
  }

  result = close(fd);
  if(result == -1) {
    cerr << "Close error: " << " Errcode:" << strerror(errno) << endl;  
    abort();
  }

  //  cerr << tid << " Wrote " << count << " items into file" << endl;
  //  cerr << tid << " File size: " << fileEntryCount_[tid] << endl;
  
  fileEntryCount_[tid] += count;
  assert(fileEntryCount_[tid] >= 0);
  ReleaseLock(locks_[tid]);
  //  cerr << tid << " File has " << fileEntryCount_[tid] << " entries" << endl;
  //  cerr << tid << " Done Writing produce buffer into file: " << count << " entries" <<endl;
}

void BufferManager::readFileIntoConsumeBuffer(THREADID tid)
{  
  //  cerr << tid << " Read file into produce buffer" << endl;
  //  cerr << tid << " File has " << fileEntryCount_[tid] << " entries" << endl;
  GetLock(locks_[tid], tid+1);
  int result;

  int fd = open(fileNames_[tid].c_str(), O_RDONLY);
  if(fd == -1) {
    cerr << "Opened to read: " << fileNames_[tid].c_str();
    cerr << "Pipe open error: " << fd << " Errcode:" << strerror(errno) << endl;
    abort();
  }

  int fd_bogus = open(bogusNames_[tid].c_str(), O_WRONLY | O_CREAT, 0777);
  if(fd_bogus == -1) {
    cerr << "Opened to write: " << bogusNames_[tid].c_str();
    cerr << "Pipe open error: " << fd_bogus << " Errcode:" << strerror(errno) << endl;
    abort();
  }

  int count = 0;
  bool validRead = true;
  while(!consumeBuffer_[tid]->full()) {
    handshake_container_t* handshake = consumeBuffer_[tid]->get_buffer();
    validRead = readHandshake(fd, handshake);    
    if(validRead == false) {
      handshake->clear();
      break;
    }
    consumeBuffer_[tid]->push_done();
    count++;
  }
  
  //  cerr << tid << " Read " << count << " entries into read buffer" << endl;
  //  cerr << tid << " Starting the rest of file move into bogus file" << endl;

  // optimize and don't allocate new handshake every time
  // need to clear the membuffer
  // same with writing
  int copyCount = 0;
  handshake_container_t handshake;
  while(validRead) {
    validRead = readHandshake(fd, &handshake);
    if(validRead) {
      writeHandshake(fd_bogus, &handshake);
      copyCount++;
    }
    else {
      break;
    }
  }

  result = close(fd_bogus);
  if(result == -1) {
    cerr << "Close error: " << " Errcode:" << strerror(errno) << endl;  
    abort();
  }
  result = close(fd);
  if(result == -1) {
    cerr << "Close error: " << " Errcode:" << strerror(errno) << endl;  
    abort();
  }

  //  cerr << tid << " Copied " << copyCount << " items to bogus file" << endl;
  //  cerr << tid << " Copy Move file back to real file" << endl;
  
  result = rename(bogusNames_[tid].c_str(), fileNames_[tid].c_str());
  if(result == -1) {
    cerr << "Can't rename filesystem bridge files. " << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  
  //  cerr << tid << " Read " << count << " items into read buffer" << endl;
  //  cerr << tid << " File size: " << fileEntryCount_[tid] << endl;
  fileEntryCount_[tid] -= count;

  assert(fileEntryCount_[tid] >= 0);
  ReleaseLock(locks_[tid]);
  //  cerr << tid << " File has " << fileEntryCount_[tid] << " entries" << endl;
  //  cerr << tid << " Done Read file into produce buffer" << endl;
}

void BufferManager::writeHandshake(int fd, handshake_container_t* handshake)
{
  int mapSize = handshake->mem_buffer.size();    
  const int handshakeBytes = sizeof(P2Z_HANDSHAKE);
  const int flagBytes = sizeof(handshake_flags_t);
  const int mapEntryBytes = sizeof(UINT32) + sizeof(UINT8);
  int mapBytes = mapSize * mapEntryBytes;
  int totalBytes = sizeof(int) + handshakeBytes + flagBytes + mapBytes;
  
  void * writeBuffer = (void*)malloc(totalBytes);
  void * buffPosition = writeBuffer;

  //  snprintf((char*)buffPosition, sizeof(int), "%d", mapSize);
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
    abort();
  }
  if(bytesWritten != totalBytes) {
    cerr << "File write error: " << bytesWritten << " expected:" << totalBytes << endl;
    abort();
  }  
  free(writeBuffer);
}

bool BufferManager::readHandshake(int fd, handshake_container_t* handshake)
{
  const int handshakeBytes = sizeof(P2Z_HANDSHAKE);
  const int flagBytes = sizeof(handshake_flags_t);
  const int mapEntryBytes = sizeof(UINT32) + sizeof(UINT8);
  
  int mapSize;
  int bytesRead = read(fd, &(mapSize), sizeof(int));
  if(bytesRead == 0) {
    return false;
  }
  if(bytesRead == -1) {
    cerr << "File read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  assert(bytesRead == sizeof(int));    
    
  int mapBytes = mapSize * mapEntryBytes;
  int totalBytes = handshakeBytes + flagBytes + mapBytes;

  void * readBuffer = (void*)malloc(totalBytes);

  bytesRead = read(fd, readBuffer, totalBytes);
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
  
  free(readBuffer);

  return true;
}
