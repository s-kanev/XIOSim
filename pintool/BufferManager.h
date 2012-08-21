#ifndef HANDSHAKE_BUFFER_MANAGER
#define HANDSHAKE_BUFFER_MANAGER

#include <map>
#include <queue>
#include <assert.h>
#include "Buffer.h"
#include "feeder.h"
#include "Buffer.h"

class BufferManager
{
 public:
  BufferManager();
  ~BufferManager();

  handshake_container_t* front(THREADID tid, bool isLocal=false);
  handshake_container_t* back(THREADID tid);
  bool empty(THREADID tid);

  // The two steps of a push -- get a buffer, do magic with
  // it, and call producer_done once it can be consumed / flushed
  // In between, back() will return a pointer to that buffer
  handshake_container_t* get_buffer(THREADID tid);
  // By assumption, we call producer_done() once we have a completely
  // instrumented, valid handshake, so that we don't need to handle
  // intermediate cases
  void producer_done(THREADID tid, bool keepLock=false);

  void pop(THREADID tid);
  void applyConsumerChanges(THREADID tid, int numChanged);
  int getConsumerSize(THREADID tid);
  void allocateThread(THREADID tid);

  bool hasThread(THREADID tid);
  unsigned int size();
  unsigned int size(THREADID tid);

  void flushBuffers(THREADID tid);

  void signalCallback(int signum);

  friend ostream& operator<< (ostream &out, handshake_container_t &hand);

 private:
  bool useRealFile_;
  bool popped_;
  
  map<THREADID, PIN_LOCK*> locks_;

  map<THREADID, int> pool_;

  map<THREADID, ofstream*> logs_;

  string gpid_;
  
  void reserveHandshake(THREADID tid);

  void copyProducerToFile(THREADID tid);
  void copyFileToConsumer(THREADID tid);

  void copyProducerToFileReal(THREADID tid);
  void copyFileToConsumerReal(THREADID tid);

  void copyProducerToFileFake(THREADID tid);
  void copyFileToConsumerFake(THREADID tid);

  bool readHandshake(THREADID tid, int fd, handshake_container_t* handshake);
  void writeHandshake(THREADID tid, int fd, handshake_container_t* handshake);

  void abort(void);
  
  void resetPool(THREADID tid);
  string genFileName();

  std::map<THREADID, int> queueSizes_;
  std::map<THREADID, Buffer*> consumeBuffer_;
  std::map<THREADID, Buffer*> produceBuffer_;
  std::map<THREADID, deque<handshake_container_t>  > fakeFile_;
  std::map<THREADID, int> fileEntryCount_;

  std::map<THREADID, deque<string> > fileNames_;
  std::map<THREADID, deque<int> > fileCounts_;

  std::map<THREADID, int> readBufferSize_;
  std::map<THREADID, void*> readBuffer_;
  std::map<THREADID, int> writeBufferSize_;
  std::map<THREADID, void*> writeBuffer_;
};

#endif
