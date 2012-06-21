#ifndef HANDSHAKE_BUFFER_MANAGER
#define HANDSHAKE_BUFFER_MANAGER

#include <map>
#include <queue>
#include <assert.h>
#include <pthread.h>
#include "feeder.h"

class BufferManager
{
 public:
  BufferManager();

  handshake_container_t* front(THREADID tid);
  handshake_container_t* back(THREADID tid);
  bool empty(THREADID tid);
  void push(THREADID tid, handshake_container_t* handshake);
  void pop(THREADID tid);
  
  bool hasThread(THREADID tid);
  unsigned int size();
  void nullifyFront(THREADID tid);

  handshake_container_t* getPooledHandshake(THREADID tid, bool justFront=false);
  void releasePooledHandshake(THREADID tid, handshake_container_t* handshake);

 private:
  map<THREADID, string> fileNames_;
  map<THREADID, string> bogusFileNames_;

  map<THREADID, int> queueSizes_;
  map<THREADID, handshake_container_t*> queueFronts_;
  map<THREADID, handshake_container_t*> queueBacks_;
  map<THREADID, int> pipeReaders_;
  map<THREADID, int> pipeWriters_;
  map<THREADID, pthread_mutex_t*> locks_;
  
  map<THREADID, handshake_queue_t> handshake_pool_;

  map<THREADID, int> currentPointer_;

  void checkFirstAccess(THREADID tid);
  void pushToFile(THREADID tid, handshake_container_t** handshake);
  void realPush(THREADID tid, handshake_container_t** handshake);
  void popFromFile(THREADID tid, handshake_container_t** handshake);

  std::map<THREADID, handshake_queue_t> handshake_buffer_;
};

#endif
