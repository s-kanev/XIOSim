#ifndef BUF_FER_H
#define BUF_FER_H

#include <cstddef>
#include <map>
#include <queue>
#include <assert.h>
#include "handshake_container.h"

class Buffer {
  public:
    Buffer(int size)
        : numPool_(size)
        , handshakePool_(new handshake_container_t[numPool_])
        , head_(0)
        , tail_(0)
        , size_(0) {}

    // Push is split in two phases: (i) a non-destructive get_buffer(),
    // which returns a pointer to an internal storage element and (ii)
    // push_done(), which markes the element as unavailable and occupying
    // space.
    handshake_container_t* get_buffer() {
        assert(!full());
        return &(handshakePool_[head_]);
    }

    void push_done() {
        assert(!full());

        head_++;
        if (head_ == numPool_) {
            head_ = 0;
        }
        size_++;
        assert(size_ <= numPool_);
    }

    void pop() {
        handshake_container_t* handshake = front();
        handshake->Clear();

        tail_++;
        if (tail_ == numPool_) {
            tail_ = 0;
        }
        size_--;
    }

    handshake_container_t* front() {
        assert(size_ > 0);
        return &(handshakePool_[tail_]);
    }

    handshake_container_t* back() {
        assert(size_ > 0);

        int dex = (head_ - 1);
        if (dex == -1) {
            dex = numPool_ - 1;
        }
        return &(handshakePool_[dex]);
    }

    // [0] corresponds to front(), [size_ - 1] to back()
    // does not bounds check
    handshake_container_t* get_item(int index) {
        int dex = (tail_ + index);
        if (dex >= numPool_)
            dex -= numPool_;
        return &(handshakePool_[dex]);
    }
    handshake_container_t* operator[](int index) { return get_item(index); }

    bool empty() { return size_ == 0; }
    bool full() { return size_ == numPool_; }

    int size() { return size_; }
    int capacity() { return numPool_; }

  private:
    int numPool_;
    handshake_container_t* handshakePool_;
    int head_;
    int tail_;

    int size_;
};

#endif
