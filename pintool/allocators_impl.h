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

/* Locally optimal allocator that waits for all loops to align before making the
 * next allocation decision.
 */
class LocallyOptimalAllocator : public BaseAllocator {
  public:
    LocallyOptimalAllocator(int num_cores);
    ~LocallyOptimalAllocator();
    int AllocateCoresForLoop(
        std::string loop_name, int asid, int* num_cores_alloc);
  private:
    // Stores a loop name and the number of cores allocated for the loop.
    typedef std::pair<std::string, int> loop_alloc_pair;
    std::map<int, loop_alloc_pair> *process_alloc_map;
    struct process_sync_t {
      int num_checked_in;
      int num_checked_out;
      bool allocation_complete;
    } process_sync;

    // Construct/reset all class global variables for sharing among threads.
    void ResetState();
};


}  // namespace xiosim

#endif
