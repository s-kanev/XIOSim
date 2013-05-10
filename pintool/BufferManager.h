#ifndef HANDSHAKE_BUFFER_MANAGER
#define HANDSHAKE_BUFFER_MANAGER

class handshake_container_t;

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
  
  map<THREADID, XIOSIM_LOCK*> locks_;

  map<THREADID, int> pool_;

  map<THREADID, ofstream*> logs_;

  string gpid_;
  
  vector<string> bridgeDirs_;
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
  
  string genFileName(string path);
  int getKBFreeSpace(string path);

 
  std::map<THREADID, int64_t> queueSizes_;
  std::map<THREADID, Buffer*> fakeFile_;
  std::map<THREADID, Buffer*> consumeBuffer_;
  std::map<THREADID, Buffer*> produceBuffer_;
  std::map<THREADID, int> fileEntryCount_;

  std::map<THREADID, deque<string> > fileNames_;
  std::map<THREADID, deque<int> > fileCounts_;

  std::map<THREADID, int> readBufferSize_;
  std::map<THREADID, void*> readBuffer_;
  std::map<THREADID, int> writeBufferSize_;
  std::map<THREADID, void*> writeBuffer_;
};

#endif
