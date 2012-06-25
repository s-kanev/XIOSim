#include "BufferManager.h"


#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

BufferManager::BufferManager()
{  
}

void BufferManager::threadDone(THREADID tid)
{
  string cmd = "/bin/rm -f " + fileNames_[tid];
  cerr << "Executing " << cmd << endl;
  system(cmd.c_str());
  
  cmd = "/bin/rm -f " + bogusNames_[tid];
  cerr << "Executing " << cmd << endl;
  system(cmd.c_str());
}

BufferManager::~BufferManager()
{
  cerr << "DESTRUCTING BUFFER MANAGER" << endl;
  map<THREADID, string>::iterator it;
  for(it = fileNames_.begin(); it != fileNames_.end(); it++) {

  }
}

handshake_container_t* BufferManager::front(THREADID tid)
{
  checkFirstAccess(tid);
  assert(queueSizes_[tid] > 0);

  if(consumeBuffer_[tid]->size() > 0) {
    consumeBuffer_[tid]->front()->flags.valid = true;
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
  consumeBuffer_[tid]->front()->flags.valid = true;
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

void BufferManager::push(THREADID tid, handshake_container_t* handshake, bool fromILDJIT)
{
  checkFirstAccess(tid);

  if((didFirstInsn_.count(tid) == 0) && (!fromILDJIT)) {    
    handshake->flags.isFirstInsn = true;
    didFirstInsn_[tid] = true;
  }
  else {// this else might be wrong...
    handshake->flags.isFirstInsn = false;
  }

  produceBuffer_[tid]->push(handshake);
  queueSizes_[tid]++;
  
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
  while(fileEntryCount_[tid] >= 100000) {
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
    sync();
    writeProduceBufferIntoFile(it->first, true);
    sync();
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

    int fd = open(fileNames_[tid].c_str(), O_WRONLY | O_CREAT, 0777);
    int result = close(fd);
    if(result == -1) {
      cerr << "Close error: " << " Errcode:" << strerror(errno) << endl;  
      abort();
    }
    sync();
    
    
    queueSizes_[tid] = 0;

    produceBuffer_[tid] = new Buffer();
    consumeBuffer_[tid] = new Buffer();       
    
    fileEntryCount_[tid] = 0;
    
    locks_[tid] = new pthread_mutex_t();
    pthread_mutex_init(locks_[tid], 0);
  }
}

void BufferManager::writeProduceBufferIntoFile(THREADID tid, bool all)
{
  //  cerr << tid << " Writing produce buffer into file" << endl;
  //  cerr << tid << " File has " << fileEntryCount_[tid] << " entries" << endl;
  pthread_mutex_lock(locks_[tid]);
  handshake_container_t* handshake;
  int result;

  int fd = open(fileNames_[tid].c_str(), O_WRONLY | O_APPEND);
  if(fd == -1) {
    cerr << "Opened to write: " << fileNames_[tid].c_str();
    cerr << "Pipe open error: " << fd << " Errcode:" << strerror(errno) << endl;
    abort();
  }

  int count = 0;
  while(produceBuffer_[tid]->size() > 1 || (produceBuffer_[tid]->size() > 0 && all)) {
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
  sync();

  //  cerr << tid << " Wrote " << count << " items into file" << endl;
  //  cerr << tid << " File size: " << fileEntryCount_[tid] << endl;
  
  fileEntryCount_[tid] += count;
  assert(fileEntryCount_[tid] >= 0);
  pthread_mutex_unlock(locks_[tid]);
  //  cerr << tid << " File has " << fileEntryCount_[tid] << " entries" << endl;
  //  cerr << tid << " Done Writing produce buffer into file: " << count << " entries" <<endl;
}

void BufferManager::readFileIntoConsumeBuffer(THREADID tid)
{  
  //  cerr << tid << " Read file into produce buffer" << endl;
  //  cerr << tid << " File has " << fileEntryCount_[tid] << " entries" << endl;
  pthread_mutex_lock(locks_[tid]);
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
    handshake_container_t handshake;
    validRead = readHandshake(fd, &handshake);    
    if(validRead == false) {
      break;
    }
    consumeBuffer_[tid]->push(&(handshake));
    count++;
  }
  
  //  cerr << tid << " Read " << count << " entries into read buffer" << endl;
  //  cerr << tid << " Starting the rest of file move into bogus file" << endl;

  // optimize and don't allocate new handshake every time
  // need to clear the membuffer
  // same with writing
  int copyCount = 0;  
  while(validRead) {
    handshake_container_t handshake;
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
  
  sync();
  
  result = rename(bogusNames_[tid].c_str(), fileNames_[tid].c_str());
  if(result == -1) {
    cerr << "Can't rename filesystem bridge files. " << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  
  sync();

  //  cerr << tid << " Read " << count << " items into read buffer" << endl;
  //  cerr << tid << " File size: " << fileEntryCount_[tid] << endl;
  fileEntryCount_[tid] -= count;

  if(fileEntryCount_[tid] < 0) {
    cerr << tid << " WARNING: fileEntryCount_[tid] < 0!!" << endl;
    cerr << "Count:" << count << endl;
    cerr << "CopyCount:" << copyCount << endl;
    cerr << "FileEntryCount_[tid]: " << fileEntryCount_[tid] << endl;
    cerr << "ConsumeBuffer Size:" << consumeBuffer_[tid]->size() << endl;
    cerr << "ProduceBuffer Size:" << produceBuffer_[tid]->size() << endl;
    cerr << endl;
  }
  //  assert(fileEntryCount_[tid] >= 0);
  pthread_mutex_unlock(locks_[tid]);
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

bool BufferManager::isFirstInsn(THREADID tid)
{
  bool isFirst = (didFirstInsn_.count(tid) == 0);
  //  didFirstInsn_[tid] = true;
  return isFirst;
}
