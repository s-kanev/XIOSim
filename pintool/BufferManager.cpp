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

handshake_container_t* BufferManager::front(THREADID tid)
{
  checkFirstAccess(tid);
  assert(queueSizes_[tid] > 0);

  if(consumeBuffer_[tid]->size() > 0) {
    cerr << "Returning from fronttop() " << tid << " with pc=" << consumeBuffer_[tid]->front()->handshake.pc << endl;
    consumeBuffer_[tid]->front()->valid = true;
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
  cerr << "Returning from frontbot() " << tid << " with pc=" << consumeBuffer_[tid]->front()->handshake.pc << endl;
  consumeBuffer_[tid]->front()->valid = true;
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
  cerr << "Pushing on " << tid << " with pc=" << handshake->handshake.pc << endl;
  
  checkFirstAccess(tid);

  if((didFirstInsn_.count(tid) == 0) && (!fromILDJIT)) {    
    handshake->isFirstInsn = true;
    didFirstInsn_[tid] = true;
  }
  else {// this else might be wrong...
    handshake->isFirstInsn = false;
  }

  produceBuffer_[tid]->push(handshake);
  queueSizes_[tid]++;
  
  if(produceBuffer_[tid]->full()) {
    writeProduceBufferIntoFile(tid);
  }
}

void BufferManager::flushBuffers(THREADID tid)
{
  cerr << "FLUSH_BUFFERS:" << tid << endl;
  map<THREADID, string>::iterator it;
  for(it = fileNames_.begin(); it != fileNames_.end(); it++) {
    cerr << "FLUSHWRITE:" << it->first << endl;
    writeProduceBufferIntoFile(it->first, true);
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
  handshake->isFirstInsn = false;
 
  cerr << "Popping on " << tid  << " with pc=" << handshake->handshake.pc << endl;
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
    
    PIN_Yield();
    sleep(5);
    PIN_Yield();
        
    fileNames_[tid] = tempnam("/dev/shm/", "HEXA");
    bogusNames_[tid] = tempnam("/dev/shm/", "HEXB");

    system(("/bin/rm -f " + fileNames_[tid]).c_str());
    system(("/bin/touch " + fileNames_[tid]).c_str());
    system(("/bin/chmod 777 " + fileNames_[tid]).c_str());

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
  cerr << "Writing buffer to file: " << tid << endl;
  //  pthread_mutex_lock(locks_[tid]);
  handshake_container_t* handshake;
  
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
    handshake->isFirstInsn = false;
    produceBuffer_[tid]->pop();
    count++;
  }

  close(fd);
  sync();

  cerr << "Wrote " << count << " items to file for tid:" << tid << endl;
  cerr << "File entry count: " << fileEntryCount_[tid] << endl;
  fileEntryCount_[tid] += count;
  cerr << "File entry count: " << fileEntryCount_[tid] << endl;
  assert(fileEntryCount_[tid] >= 0);
  //  pthread_mutex_unlock(locks_[tid]);
}

void BufferManager::readFileIntoConsumeBuffer(THREADID tid)
{
  //  pthread_mutex_lock(locks_[tid]);
  cerr << "Read file into buffer after_lock: " << tid << endl;
  
  int fd = open(fileNames_[tid].c_str(), O_RDONLY);
  if(fd == -1) {
    cerr << "Opened to read: " << fileNames_[tid].c_str();
    cerr << "Pipe open error: " << fd << " Errcode:" << strerror(errno) << endl;
    abort();
  }

  int count = 0;
  bool doBreak = false;
  while((!consumeBuffer_[tid]->full()) || doBreak) {
    handshake_container_t handshake;
    bool validRead = readHandshake(fd, &handshake);
    if(validRead == false) {
      cerr << "I break!" << endl;
      break;
    }
    consumeBuffer_[tid]->push(&(handshake));
    count++;
  }
  cerr << "how many?" << endl;
  system(("/bin/rm -f " + bogusNames_[tid]).c_str());
  system(("/bin/touch " + bogusNames_[tid]).c_str());
  system(("/bin/chmod 777 " + bogusNames_[tid]).c_str());
  int fd_bogus = open(bogusNames_[tid].c_str(), O_WRONLY | O_APPEND);
  if(fd_bogus == -1) {
    cerr << "Opened to write: " << bogusNames_[tid].c_str();
    cerr << "Pipe open error: " << fd_bogus << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  
  handshake_container_t handshake;
  bool validRead = readHandshake(fd, &handshake);
  int copyCount = 0;  
  while(validRead == true) {
    writeHandshake(fd_bogus, &handshake);
    validRead = readHandshake(fd, &handshake);
    copyCount++;
  }
  close(fd_bogus);
  close(fd);

  sync();
  system(("/bin/cp -rf " + bogusNames_[tid] + " " + fileNames_[tid]).c_str());
  sync();

  fileEntryCount_[tid] -= count;

  cerr << "Read " << count << " items from file for tid:" << tid << endl;
  cerr << "Copied " << copyCount << " items from file to file for tid:" << tid << endl;
  cerr << "File entry count: " << fileEntryCount_[tid] << endl;

  assert(fileEntryCount_[tid] >= 0);
  //  pthread_mutex_unlock(locks_[tid]);
}

void BufferManager::writeHandshake(int fd, handshake_container_t* handshake)
{
  int bytesWritten = write(fd, &(handshake->handshake), sizeof(P2Z_HANDSHAKE));
  if(bytesWritten == -1) {
    cerr << "Pipe write error: " << bytesWritten << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  if(bytesWritten != sizeof(P2Z_HANDSHAKE)) {
    cerr << "Pipe write error: " << bytesWritten << endl;
  }
  assert(bytesWritten == sizeof(P2Z_HANDSHAKE));
 
  bytesWritten = write(fd, &(handshake->valid), sizeof(BOOL));
  if(bytesWritten == -1) {
    cerr << "Pipe write error: " << bytesWritten << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  assert(bytesWritten == sizeof(BOOL));

  bytesWritten = write(fd, &(handshake->mem_released), sizeof(BOOL));
  if(bytesWritten == -1) {
    cerr << "Pipe write error: " << bytesWritten << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  assert(bytesWritten == sizeof(BOOL));

  bytesWritten = write(fd, &(handshake->isFirstInsn), sizeof(BOOL));
  if(bytesWritten == -1) {
    cerr << "Pipe write error: " << bytesWritten << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  assert(bytesWritten == sizeof(BOOL));

  bytesWritten = write(fd, &(handshake->isLastInsn), sizeof(BOOL));
  if(bytesWritten == -1) {
    cerr << "Pipe write error: " << bytesWritten << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  assert(bytesWritten == sizeof(BOOL));

  bytesWritten = write(fd, &(handshake->killThread), sizeof(BOOL));
  if(bytesWritten == -1) {
    cerr << "Pipe write error: " << bytesWritten << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  assert(bytesWritten == sizeof(BOOL));

  int mapSize = handshake->mem_buffer.size();
  bytesWritten = write(fd, &(mapSize), sizeof(int));
  if(bytesWritten == -1) {
    cerr << "Pipe write error: " << bytesWritten << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  assert(bytesWritten == sizeof(int)); 
  
  map<UINT32, UINT8>::iterator it;
  for(it = handshake->mem_buffer.begin(); it != handshake->mem_buffer.end(); it++) {
    bytesWritten = write(fd, &(it->first), sizeof(UINT32));
    if(bytesWritten == -1) {
      cerr << "Pipe write error: " << bytesWritten << " Errcode:" << strerror(errno) << endl;
      abort();
    }
    assert(bytesWritten == sizeof(UINT32));
    
    bytesWritten = write(fd, &(it->second), sizeof(UINT8));
    if(bytesWritten == -1) {
      cerr << "Pipe write error: " << bytesWritten << " Errcode:" << strerror(errno) << endl;
      abort();
    }
    assert(bytesWritten == sizeof(UINT8));
  } 
}

bool BufferManager::readHandshake(int fd, handshake_container_t* handshake)
{
  int bytesRead = -1;
  
  bytesRead = read(fd, &(handshake->handshake), sizeof(P2Z_HANDSHAKE));
  
  if(bytesRead == 0) {
    return false;
  }
  if(bytesRead == -1) {
    cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  
  assert(bytesRead == sizeof(P2Z_HANDSHAKE));    
  
  bytesRead = read(fd, &(handshake->valid), sizeof(BOOL));
  if(bytesRead == -1) {
    cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
    abort();
  }    
  assert(bytesRead == sizeof(BOOL));
  
  bytesRead = read(fd, &(handshake->mem_released), sizeof(BOOL));
  if(bytesRead == -1) {
    cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
    abort();
  }    
  assert(bytesRead == sizeof(BOOL));
  
  bytesRead = read(fd, &(handshake->isFirstInsn), sizeof(BOOL));
  if(bytesRead == -1) {
    cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
    abort();
  }    
  assert(bytesRead == sizeof(BOOL));
  
  bytesRead = read(fd, &(handshake->isLastInsn), sizeof(BOOL));
  if(bytesRead == -1) {
    cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
    abort();
  }    
  assert(bytesRead == sizeof(BOOL));
  
  bytesRead = read(fd, &(handshake->killThread), sizeof(BOOL));
  if(bytesRead == -1) {
    cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
    abort();
  }    
  assert(bytesRead == sizeof(BOOL));
  
  int mapSize;
  bytesRead = read(fd, &(mapSize), sizeof(int));
  if(bytesRead == -1) {
    cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
    abort();
  }    
  assert(bytesRead == sizeof(int));
  
  for(int i = 0; i < mapSize; i++) {
    UINT32 first;
    UINT8 second;
    
    bytesRead = read(fd, &(first), sizeof(UINT32));
    if(bytesRead == -1) {
      cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
      abort();
    }      
    assert(bytesRead == sizeof(UINT32));
    
    bytesRead = read(fd, &(second), sizeof(UINT8));
    if(bytesRead == -1) {
      cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
      abort();
    }      
    assert(bytesRead == sizeof(UINT8));
    
    (handshake->mem_buffer)[first] = second;
  }
  
  return true;
}

/*handshake_container_t* BufferManager::getPooledHandshake(THREADID tid, bool fromILDJIT)
{
  checkFirstAccess(tid);    

  handshake_queue_t *pool;
  pool = &(handshake_pool_[tid]);

  long long int spins = 0;  
  while(pool->empty()) {
    ReleaseLock(&simbuffer_lock);
    PIN_Yield();
    GetLock(&simbuffer_lock, tid+1);
    spins++;
    if(spins >= 7000000LL) {
      cerr << tid << " [getPooledHandshake()]: That's a lot of spins!" << endl;
      spins = 0;
    }
  }

  handshake_container_t* result = pool->front();
  ASSERTX(result != NULL);

  pool->pop();
  
  if((didFirstInsn_.count(tid) == 0) && (!fromILDJIT)) {    
    result->isFirstInsn = true;
    didFirstInsn_[tid] = true;
  }
  else {// this else might be wrong...
    result->isFirstInsn = false;
  }

  result->mem_released = true;
  result->valid = false;
  
  return result;
}

void BufferManager::releasePooledHandshake(THREADID tid, handshake_container_t* handshake)
{
  handshake_pool_[tid].push(handshake);
  return;
}
*/
bool BufferManager::isFirstInsn(THREADID tid)
{
  bool isFirst = (didFirstInsn_.count(tid) == 0);
  //  didFirstInsn_[tid] = true;
  return isFirst;
}
