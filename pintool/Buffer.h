#ifndef BUF_FER_H
#define BUF_FER_H

#include <map>
#include <queue>
#include <assert.h>
#include "feeder.h"

class Buffer
{
 public:
  Buffer();
  
  handshake_container_t* push();
  void pop();
  
  handshake_container_t* front();
  handshake_container_t* back();

  bool empty();
  bool full();
  
  int size();
  
 private:
  handshake_container_t** handshakePool_;
  int head_;
  int tail_;

  int size_;
  int numPool_;
};

#endif
