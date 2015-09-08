#ifndef BUF_FER_H
#define BUF_FER_H

#include <cstddef>
#include <assert.h>

/* Buffer is a pre-allocated circular queue.
 * T must be default-constructible, and have a void Invalidate(void) method. */
template<typename T>
class Buffer {
  public:
    Buffer(int size)
        : numPool_(size)
        , pool_(new T[numPool_])
        , head_(0)
        , tail_(0)
        , size_(0) {}

    // Push is split in two phases: (i) a non-destructive get_buffer(),
    // which returns a pointer to an internal storage element and (ii)
    // push_done(), which markes the element as unavailable and occupying
    // space.
    T* get_buffer() {
        assert(!full());
        return &(pool_[head_]);
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
        T* head = front();
        head->Invalidate();

        tail_++;
        if (tail_ == numPool_) {
            tail_ = 0;
        }
        size_--;
    }

    void pop_back() {
        T* head = back();
        head->Invalidate();

        head_--;
        if (head_ == -1) {
            head_ = numPool_ - 1;
        }
        size_--;
    }

    T* front() {
        assert(size_ > 0);
        return &(pool_[tail_]);
    }

    T* back() {
        assert(size_ > 0);

        int dex = (head_ - 1);
        if (dex == -1) {
            dex = numPool_ - 1;
        }
        return &(pool_[dex]);
    }

    // [0] corresponds to front(), [size_ - 1] to back()
    // does not bounds check
    T* get_item(int index) {
        int dex = (tail_ + index);
        if (dex >= numPool_)
            dex -= numPool_;
        return &(pool_[dex]);
    }
    T* operator[](int index) { return get_item(index); }

    bool empty() { return size_ == 0; }
    bool full() { return size_ == numPool_; }

    int size() { return size_; }
    int capacity() { return numPool_; }

  private:
    int numPool_;
    T* pool_;
    int head_;
    int tail_;

    int size_;
};

#endif
