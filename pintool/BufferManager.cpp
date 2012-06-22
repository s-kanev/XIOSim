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
  return queueFronts_[tid];
}

handshake_container_t* BufferManager::back(THREADID tid)
{
  checkFirstAccess(tid);
  assert(queueSizes_[tid] > 0);

  if(queueBacks_[tid] != NULL) {
    return queueBacks_[tid];
  }
  
  // if only one element in queue, it will be in queueFronts, 
  // even though it is back()
  assert(queueFronts_[tid] != NULL);
  return queueFronts_[tid];
}

bool BufferManager::empty(THREADID tid)
{
  checkFirstAccess(tid);    
  return queueSizes_[tid] == 0;
  return handshake_buffer_[tid].empty();
}

void BufferManager::push(THREADID tid, handshake_container_t* handshake)
{ 
  checkFirstAccess(tid);
  if(queueSizes_[tid] == 0) {
    queueFronts_[tid] = handshake;
  }
  else if(queueSizes_[tid] == 1) {
    queueBacks_[tid] = handshake;
  }
  else {
    assert(queueBacks_[tid] != NULL);
    pushToFile(tid, &(queueBacks_[tid]));
    queueBacks_[tid] = handshake;
  }

  queueBacks_[tid] = handshake;
  queueSizes_[tid]++;
}

void BufferManager::pop(THREADID tid)
{
  checkFirstAccess(tid);
  assert(queueSizes_[tid] > 0);
  
  if(queueSizes_[tid] == 1) {
    queueFronts_[tid] = NULL;
  }
  else if(queueSizes_[tid] == 2) {
    queueFronts_[tid] = queueBacks_[tid];
    queueBacks_[tid] = NULL;
  }
  else {
    popFromFile(tid, &(queueFronts_[tid]));
  }
  
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

void BufferManager::nullifyFront(THREADID tid)
{
  assert(false);
  checkFirstAccess(tid);    
  //  handshake_buffer_[tid].front() = NULL;

  // TODO: grab new front from file i guess
}


void BufferManager::checkFirstAccess(THREADID tid)
{
  if(queueSizes_.count(tid) == 0) {
    char str[50];
    sprintf(str, "%d", tid);
    
    fileNames_[tid] = tempnam("/dev/shm/", "sima");
    bogusFileNames_[tid] = tempnam("/dev/shm/", "simb");

    system(("/bin/rm -f " + fileNames_[tid]).c_str());
    system(("/bin/touch " + fileNames_[tid]).c_str());
    system(("/bin/chmod 777 " + fileNames_[tid]).c_str());

    queueSizes_[tid] = 0;
    queueFronts_[tid] = NULL;
    queueBacks_[tid] = NULL;
    
    for (int i=0; i < 2000; i++) {
      handshake_container_t* new_handshake = new handshake_container_t();
      if (i > 0) {
	new_handshake->isFirstInsn = false;            
      }
      else {
	new_handshake->isFirstInsn = true;
      }
      handshake_pool_[tid].push(new_handshake);      
    }

    locks_[tid] = new pthread_mutex_t();
    pthread_mutex_init(locks_[tid], 0);
  }
}

void BufferManager::pushToFile(THREADID tid, handshake_container_t** handshake)
{
  pthread_mutex_lock(locks_[tid]);
  pipeWriters_[tid] = open(fileNames_[tid].c_str(), O_WRONLY | O_APPEND);
  if(pipeWriters_[tid] == -1) {
    cerr << "Opened to write: " << fileNames_[tid].c_str();
    cerr << "Pipe open error: " << pipeWriters_[tid] << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  
  realPush(tid, handshake);

  close(pipeWriters_[tid]);
  sync();
  pthread_mutex_unlock(locks_[tid]);
}

void BufferManager::realPush(THREADID tid, handshake_container_t** handshake)
{
  int bytesWritten = write(pipeWriters_[tid], &((*handshake)->handshake), sizeof(P2Z_HANDSHAKE));
  if(bytesWritten == -1) {
    cerr << "Pipe write error: " << bytesWritten << " Errcode:" << strerror(errno) << endl;
    abort();
  }
  if(bytesWritten != sizeof(P2Z_HANDSHAKE)) {
    cerr << "Pipe write error: " << bytesWritten << endl;
  }
  assert(bytesWritten == sizeof(P2Z_HANDSHAKE));
 
  bytesWritten = write(pipeWriters_[tid], &((*handshake)->valid), sizeof(BOOL));
  assert(bytesWritten == sizeof(BOOL));

  bytesWritten = write(pipeWriters_[tid], &((*handshake)->mem_released), sizeof(BOOL));
  assert(bytesWritten == sizeof(BOOL));

  bytesWritten = write(pipeWriters_[tid], &((*handshake)->isFirstInsn), sizeof(BOOL));
  assert(bytesWritten == sizeof(BOOL));

  bytesWritten = write(pipeWriters_[tid], &((*handshake)->isLastInsn), sizeof(BOOL));
  assert(bytesWritten == sizeof(BOOL));

  bytesWritten = write(pipeWriters_[tid], &((*handshake)->killThread), sizeof(BOOL));
  assert(bytesWritten == sizeof(BOOL));

  int mapSize = (*handshake)->mem_buffer.size();
  bytesWritten = write(pipeWriters_[tid], &(mapSize), sizeof(int));
  assert(bytesWritten == sizeof(int));
  
  map<UINT32, UINT8>::iterator it;
  for(it = (*handshake)->mem_buffer.begin(); it != (*handshake)->mem_buffer.end(); it++) {
    bytesWritten = write(pipeWriters_[tid], &(it->first), sizeof(UINT32));
    assert(bytesWritten == sizeof(UINT32));
    
    bytesWritten = write(pipeWriters_[tid], &(it->second), sizeof(UINT8));
    assert(bytesWritten == sizeof(UINT8));
  } 
}

void BufferManager::popFromFile(THREADID tid, handshake_container_t** handshake)
{
  pthread_mutex_lock(locks_[tid]);
  system(("/bin/rm -f " + bogusFileNames_[tid]).c_str());
  system(("/bin/touch " + bogusFileNames_[tid]).c_str());
  system(("/bin/chmod 777 " + bogusFileNames_[tid]).c_str());

  pipeReaders_[tid] = open(fileNames_[tid].c_str(), O_RDONLY);
  if(pipeReaders_[tid] == -1) {
    cerr << "Opened to read: " << fileNames_[tid].c_str();
    cerr << "Pipe open error: " << pipeReaders_[tid] << " Errcode:" << strerror(errno) << endl;
    abort();
  }

  int bytesRead = -1;
  handshake_container_t** readHandshake;
  handshake_container_t bogusHandshake;
  handshake_container_t* bogusHandshakePointer = &bogusHandshake;
  int count = 0;

  while(true) {
    if(count == 0) {
      readHandshake = handshake;
    }
    else {
      readHandshake = &(bogusHandshakePointer);
    }
    bytesRead = read(pipeReaders_[tid], &((*readHandshake)->handshake), sizeof(P2Z_HANDSHAKE));
    if(bytesRead == 0) {
      break;
    }    
    if(bytesRead == -1) {
      cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
      abort();
    }

    assert(bytesRead == sizeof(P2Z_HANDSHAKE));    

    bytesRead = read(pipeReaders_[tid], &((*readHandshake)->valid), sizeof(BOOL));
if(bytesRead == -1) {
      cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
      abort();
    }    
assert(bytesRead == sizeof(BOOL));

    bytesRead = read(pipeReaders_[tid], &((*readHandshake)->mem_released), sizeof(BOOL));
if(bytesRead == -1) {
      cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
      abort();
    }    
assert(bytesRead == sizeof(BOOL));
    
    bytesRead = read(pipeReaders_[tid], &((*readHandshake)->isFirstInsn), sizeof(BOOL));
if(bytesRead == -1) {
      cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
      abort();
    }    
assert(bytesRead == sizeof(BOOL));
    
    bytesRead = read(pipeReaders_[tid], &((*readHandshake)->isLastInsn), sizeof(BOOL));
if(bytesRead == -1) {
      cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
      abort();
    }    
assert(bytesRead == sizeof(BOOL));
    
    bytesRead = read(pipeReaders_[tid], &((*readHandshake)->killThread), sizeof(BOOL));
if(bytesRead == -1) {
      cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
      abort();
    }    
assert(bytesRead == sizeof(BOOL));

    int mapSize;
    bytesRead = read(pipeReaders_[tid], &(mapSize), sizeof(int));
if(bytesRead == -1) {
      cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
      abort();
    }    
assert(bytesRead == sizeof(int));
    
    for(int i = 0; i < mapSize; i++) {
      UINT32 first;
      UINT8 second;
      
      bytesRead = read(pipeReaders_[tid], &(first), sizeof(UINT32));
if(bytesRead == -1) {
      cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
      abort();
    }      
assert(bytesRead == sizeof(UINT32));
      
      bytesRead = read(pipeReaders_[tid], &(second), sizeof(UINT8));
if(bytesRead == -1) {
      cerr << "Pipe read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
      abort();
    }      
 assert(bytesRead == sizeof(UINT8));
      
      ((*readHandshake)->mem_buffer)[first] = second;
    }    
    
    if(count > 0) {
      pipeWriters_[tid] = open(bogusFileNames_[tid].c_str(), O_WRONLY | O_APPEND);
      realPush(tid, readHandshake);
      close(pipeWriters_[tid]);
    }
    
    count++;    
  }
  
  close(pipeReaders_[tid]);    
  
  sync();
  
  string cmd = "/bin/cp -rf " +bogusFileNames_[tid] + " " + fileNames_[tid];
  system(cmd.c_str());
  
  sync();
  
  pthread_mutex_unlock(locks_[tid]);
}

handshake_container_t* BufferManager::getPooledHandshake(THREADID tid)
{
  handshake_queue_t *pool;
  pool = &(handshake_pool_[tid]);

  long long int spins = 0;  
  while(pool->empty()) {
    ReleaseLock(&simbuffer_lock);
    PIN_Yield();
    GetLock(&simbuffer_lock, tid+1);
    spins++;
    if(spins >= 70000LL) {
      cerr << "[getPooledHandshake()]: That's a lot of spins!" << endl;
      spins = 0;
    }
  }

  handshake_container_t* result = pool->front();
  ASSERTX(result != NULL);

  pool->pop();
    
  return result;
}

void BufferManager::releasePooledHandshake(THREADID tid, handshake_container_t* handshake)
{
  handshake_pool_[tid].push(handshake);
  return;
}
