#ifndef HANDSHAKE_BUFFER_MANAGER
#define HANDSHAKE_BUFFER_MANAGER

#include <map>
#include <queue>
#include <assert.h>
#include <pthread.h>
#include "feeder.h"
#include "Buffer.h"

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
  map<THREADID, PIN_LOCK*> locks_;
  
  map<THREADID, int> didFirstInsn_;
  map<THREADID, int> pool_;

  map<THREADID, ofstream*> logs_;

  void checkFirstAccess(THREADID tid);
  void getPooledHandshake(THREADID tid, bool fromILDJIT=false);

  void copyProducerToFile(THREADID tid, bool all);
  void copyFileToConsumer(THREADID tid);

  std::map<THREADID, int> queueSizes_;
  std::map<THREADID, Buffer*> consumeBuffer_;
  std::map<THREADID, Buffer*> produceBuffer_;
  std::map<THREADID, Buffer*> fakeFile_;
};

#endif
