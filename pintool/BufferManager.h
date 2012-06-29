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
  bool useRealFile_;
  map<THREADID, PIN_LOCK*> locks_;
  
  map<THREADID, int> didFirstInsn_;
  map<THREADID, int> pool_;

  map<THREADID, ofstream*> logs_;

  void checkFirstAccess(THREADID tid);

  void reserveHandshake(THREADID tid, bool fromILDJIT=false);

  void copyProducerToFile(THREADID tid, bool all);
  void copyFileToConsumer(THREADID tid);

  void copyProducerToFileReal(THREADID tid, bool all);
  void copyFileToConsumerReal(THREADID tid);

  void copyProducerToFileFake(THREADID tid, bool all);
  void copyFileToConsumerFake(THREADID tid);

  bool readHandshake(int fd, handshake_container_t* handshake);
  void writeHandshake(int fd, handshake_container_t* handshake);
  
  std::map<THREADID, int> queueSizes_;
  std::map<THREADID, Buffer*> consumeBuffer_;
  std::map<THREADID, Buffer*> produceBuffer_;
  std::map<THREADID, Buffer*> fakeFile_;
  std::map<THREADID, int> fileEntryCount_;

  std::map<THREADID, string> fileNames_;
  std::map<THREADID, string> bogusNames_;

  
};

#endif
