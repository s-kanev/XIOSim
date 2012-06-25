#ifndef HANDSHAKE_BUFFER_MANAGER
#define HANDSHAKE_BUFFER_MANAGER

#include <map>
#include <queue>
#include <assert.h>
#include "Buffer.h"
#include "feeder.h"

class BufferManager
{
 public:
  BufferManager();
  ~BufferManager();

  handshake_container_t* front(THREADID tid);
  handshake_container_t* back(THREADID tid);
  bool empty(THREADID tid);

  handshake_container_t* push(THREADID tid, bool fromILDJIT=false);
  void pop(THREADID tid);

  bool hasThread(THREADID tid);
  unsigned int size();
  bool isFirstInsn(THREADID tid);

  void flushBuffers(THREADID tid);
  void threadDone(THREADID tid);

 private:
  map<THREADID, string> fileNames_;
  map<THREADID, string> bogusNames_;

  map<THREADID, int> queueSizes_;
  map<THREADID, Buffer*> produceBuffer_;
  map<THREADID, Buffer*> consumeBuffer_;
   
  map<THREADID, int> didFirstInsn_;
  map<THREADID, int> fileEntryCount_;

  map<THREADID, PIN_LOCK*> locks_;

  void checkFirstAccess(THREADID tid);
  void writeProduceBufferIntoFile(THREADID tid, bool all=false);
  void readFileIntoConsumeBuffer(THREADID tid);
  void writeHandshake(int fd, handshake_container_t* handshake);
  bool readHandshake(int fd, handshake_container_t* handshake);

  handshake_container_t* getPooledHandshake(THREADID tid, bool fromILDJIT=true);
  void releasePooledHandshake(THREADID tid, handshake_container_t* handshake);

  std::map<THREADID, handshake_queue_t> handshake_buffer_;
};

#endif
