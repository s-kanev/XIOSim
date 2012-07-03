#ifndef BUF_FER_H
#define BUF_FER_H

#include <map>
#include <queue>
#include <assert.h>
#include "feeder.h"

class Buffer
{
 public:
  Buffer(int size);
 
  // Push is split in two phases: (i) a non-destructive get_buffer(),
  // which returns a pointer to an internal storage element and (ii)
  // push_done(), which markes the element as unavailable and occupying
  // space.
  handshake_container_t* get_buffer();
  void push_done();

  void pop();
  
  handshake_container_t* front();
  handshake_container_t* back();

  bool empty();
  bool full();
  
  int size();
  
 private:
  handshake_container_t* handshakePool_;
  int head_;
  int tail_;

  int size_;
  int numPool_;
};

#endif
