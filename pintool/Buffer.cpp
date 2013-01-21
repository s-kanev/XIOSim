#include "Buffer.h"

Buffer::Buffer()
{
  numPool_ = 10;
  handshakePool_ = new handshake_container_t [numPool_];
  head_ = 0;
  tail_ = 0;
  size_ = 0;
}


Buffer::Buffer(int size)
{
  numPool_ = size;
  handshakePool_ = new handshake_container_t [numPool_];
  head_ = 0;
  tail_ = 0;
  size_ = 0;
}

handshake_container_t* Buffer::get_buffer()
{
  assert(!full());
  return &(handshakePool_[head_]);
}

void Buffer::push_done()
{
  assert(!full());

  head_++;
  if(head_ == numPool_) {
    head_ = 0;
  }
  size_++;
  assert(size_ <= numPool_);
}

void Buffer::pop()
{
  handshake_container_t* handshake = front();
  handshake->Clear();

  tail_++;
  if(tail_ == numPool_) {
    tail_ = 0;
  }
  size_--;
}

handshake_container_t* Buffer::front()
{
  assert(size_ > 0);
  return &(handshakePool_[tail_]);
}

handshake_container_t* Buffer::back()
{
  assert(size_ > 0);

  int dex = (head_ - 1);
  if(dex == -1 ) {
    dex = numPool_ - 1;
  }
  return &(handshakePool_[dex]);
}

// [0] corresponds to front(), [size_ - 1] to back()
// does not bounds check
handshake_container_t* Buffer::operator[](int index)
{
  int dex = (tail_ + index) % numPool_;
  return &(handshakePool_[dex]);
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

int Buffer::capacity()
{
  return numPool_;
}
