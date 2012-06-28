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

  void push(THREADID tid, handshake_container_t* handshake, bool fromILDJIT=false);
  void pop(THREADID tid, handshake_container_t* handshake);

  bool hasThread(THREADID tid);
  unsigned int size();
  bool isFirstInsn(THREADID tid);

  friend ostream& operator<< (ostream &out, handshake_container_t &hand);

 private:
  map<THREADID, pthread_mutex_t*> locks_;
  
  map<THREADID, handshake_queue_t> handshake_pool_;

  map<THREADID, int> didFirstInsn_;

  map<THREADID, ofstream*> logs_;

  void checkFirstAccess(THREADID tid);
  handshake_container_t* getPooledHandshake(THREADID tid, bool fromILDJIT=true);
  void releasePooledHandshake(THREADID tid, handshake_container_t* handshake);

  std::map<THREADID, handshake_queue_t> handshake_buffer_;
};

#endif
