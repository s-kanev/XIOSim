#ifndef __ALLOCATORS_IMPL_H__
#define __ALLOCATORS_IMPL_H__

#include "base_allocator.h"

namespace xiosim {

/* A credit-based core allocator. */
class PenaltyAllocator : public BaseAllocator {
  public:
    PenaltyAllocator(int num_cores) : BaseAllocator(num_cores) {}
    void AllocateCoresForLoop(
        char* loop_name, pid_t pid, int* num_cores_alloc);
};


}  // namespace xiosim

#endif
