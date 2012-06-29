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

  void signalHandler(int signum);
  
  handshake_container_t* front(THREADID tid);
  handshake_container_t* back(THREADID tid);
  bool empty(THREADID tid);

  // The two steps of a push -- get a buffer, do magic with
  // it, and call producer_done once it can be consumed / flushed
  // In between, back() will return a pointer to that buffer
  handshake_container_t* get_buffer(THREADID tid);
  // By assumption, we call producer_done() once we have a completely
  // instrumented, valid handshake, so that we don't need to handle
  // intermediate cases
  void producer_done(THREADID tid);

  void pop(THREADID tid);

  bool hasThread(THREADID tid);
  unsigned int size();

  void flushBuffers(THREADID tid);
  void threadDone(THREADID tid);

 private:
  map<THREADID, string> fileNames_;
  map<THREADID, string> bogusNames_;

  map<THREADID, int> queueSizes_;
  map<THREADID, Buffer*> produceBuffer_;
  map<THREADID, Buffer*> consumeBuffer_;
   
  map<THREADID, int> fileEntryCount_;

  map<THREADID, PIN_LOCK*> locks_;

  void checkFirstAccess(THREADID tid);
  void writeProduceBufferIntoFile(THREADID tid);
  void readFileIntoConsumeBuffer(THREADID tid);
  void writeHandshake(int fd, handshake_container_t* handshake);
  bool readHandshake(int fd, handshake_container_t* handshake);

  std::map<THREADID, handshake_queue_t> handshake_buffer_;
};

#endif
