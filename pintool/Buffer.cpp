#include "Buffer.h"

Buffer::Buffer()
{
  numPool_ = 2000;
  handshakePool_ = new handshake_container_t* [numPool_];

  for (int i = 0; i < numPool_; i++) {
    handshake_container_t* newHandshake = new handshake_container_t();
    if (i > 0) {
      newHandshake->isFirstInsn = false;            
    }
    else {
      newHandshake->isFirstInsn = true;
    }
    handshakePool_[i] = newHandshake;      
  }
  head_ = 0;
  tail_ = 0;
  size_ = 0;
}

void Buffer::push(handshake_container_t* handshake)
{
  assert(!full());
  
  handshake_container_t* copyTo = handshakePool_[head_];
  bool isFirstInsn = copyTo->isFirstInsn;

  copyTo->valid = handshake->valid;
  copyTo->mem_released = handshake->mem_released;
  copyTo->mem_buffer = handshake->mem_buffer;
  copyTo->isFirstInsn = handshake->isFirstInsn;
  copyTo->isLastInsn = handshake->isLastInsn;
  copyTo->killThread = handshake->killThread;

  memcpy(&(copyTo->handshake), &(handshake->handshake), sizeof(P2Z_HANDSHAKE));

  copyTo->isFirstInsn = isFirstInsn;

  head_++;
  if(head_ == numPool_) {
    head_ = 0;
  }
  size_++;
}

void Buffer::pop()
{
  front();
  tail_++;
  if(tail_ == numPool_) {
    tail_ = 0;
  }
  size_--;
}

handshake_container_t* Buffer::front()
{
  assert(size_ > 0);
  return handshakePool_[tail_];
}

handshake_container_t* Buffer::back()
{
  assert(size_ > 0);
  int dex = (head_ - 1);
  if(dex == -1 ) {
    dex = numPool_ - 1;
  }
  return handshakePool_[dex];
}

bool Buffer::empty()
{
  return size_ == 0;
}

bool Buffer::full()
{
  return size_ == numPool_;
}

int Buffer::size()
{
  return size_;
}
