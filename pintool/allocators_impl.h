#ifndef __ALLOCATORS_IMPL_H__
#define __ALLOCATORS_IMPL_H__

#include <string>
#include <vector>

#include "base_allocator.h"

namespace xiosim {

/* A credit-based core allocator. */
class PenaltyAllocator : public BaseAllocator {
  public:
    PenaltyAllocator(int num_cores);
    int AllocateCoresForLoop(
        std::string loop_name, int asid, int* num_cores_alloc);
};


}  // namespace xiosim

#endif
