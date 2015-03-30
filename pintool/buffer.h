#ifndef BUF_FER_H
#define BUF_FER_H

#include <map>
#include <queue>
#include <assert.h>
#include "handshake_container.h"

class Buffer
{
 public:
  Buffer(int size);
  Buffer();
 
  // Push is split in two phases: (i) a non-destructive get_buffer(),
  // which returns a pointer to an internal storage element and (ii)
  // push_done(), which markes the element as unavailable and occupying
  // space.
  handshake_container_t* get_buffer();
  void push_done();

  void pop();
  
  handshake_container_t* front();
  handshake_container_t* back();

  handshake_container_t* get_item(int index);
  handshake_container_t* operator[](int index);

  bool empty();
  bool full();
  
  int size();
  int capacity();
  
 private:
  handshake_container_t* handshakePool_;
  int head_;
  int tail_;

  int size_;
  int numPool_;
};

#endif
