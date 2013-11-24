#ifndef HANDSHAKE_BUFFER_MANAGER
#define HANDSHAKE_BUFFER_MANAGER

#include "../buffer.h"

class handshake_container_t;

using boost::interprocess::managed_shared_memory;

class BufferManager
{
 public:
  typedef boost::interprocess::allocator<int, managed_shared_memory::segment_manager> deque_int_allocator;
  typedef boost::interprocess::deque<int, deque_int_allocator> shm_int_deque;
  typedef boost::interprocess::allocator<boost::interprocess::string, managed_shared_memory::segment_manager> deque_allocator;
  typedef boost::interprocess::deque<boost::interprocess::string, deque_allocator> shm_string_deque;

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
  inline unsigned int numThreads()
  {
    return numThreads_;
  }

  uint64_t size(THREADID tid);

  void flushBuffers(THREADID tid);

  void signalCallback(int signum);

  void resetPool(THREADID tid);

  friend ostream& operator<< (ostream &out, handshake_container_t &hand);

 private:
  bool useRealFile_;
  int numThreads_;

  xiosim::shared::SharedUnorderedMap<THREADID, XIOSIM_LOCK*> locks_;
  xiosim::shared::SharedUnorderedMap<THREADID, int> pool_;
  xiosim::shared::SharedUnorderedMap<THREADID, ofstream*> logs_;

  boost::interprocess::string gpid_;

  vector<boost::interprocess::string> bridgeDirs_;
  bool popped_;

  void reserveHandshake(THREADID tid);

  void copyProducerToFile(THREADID tid, bool checkSpace);
  void copyFileToConsumer(THREADID tid);

  void copyProducerToFileReal(THREADID tid, bool checkSpace);
  void copyFileToConsumerReal(THREADID tid);

  void copyProducerToFileFake(THREADID tid);
  void copyFileToConsumerFake(THREADID tid);

  bool readHandshake(THREADID tid, int fd, handshake_container_t* handshake);
  void writeHandshake(THREADID tid, int fd, handshake_container_t* handshake);

  void abort(void);

  boost::interprocess::string genFileName(boost::interprocess::string path);
  int getKBFreeSpace(boost::interprocess::string path);

  xiosim::shared::SharedUnorderedMap<THREADID, int64_t> queueSizes_;
  xiosim::shared::SharedUnorderedMap<THREADID, Buffer*> fakeFile_;
  xiosim::shared::SharedUnorderedMap<THREADID, Buffer*> consumeBuffer_;
  xiosim::shared::SharedUnorderedMap<THREADID, Buffer*> produceBuffer_;
  xiosim::shared::SharedUnorderedMap<THREADID, int> fileEntryCount_;

  xiosim::shared::SharedUnorderedMap<THREADID, shm_string_deque> fileNames_;
  xiosim::shared::SharedUnorderedMap<THREADID, shm_int_deque> fileCounts_;

  xiosim::shared::SharedUnorderedMap<THREADID, int> readBufferSize_;
  xiosim::shared::SharedUnorderedMap<THREADID, void*> readBuffer_;
  xiosim::shared::SharedUnorderedMap<THREADID, int> writeBufferSize_;
  xiosim::shared::SharedUnorderedMap<THREADID, void*> writeBuffer_;
};

#endif
