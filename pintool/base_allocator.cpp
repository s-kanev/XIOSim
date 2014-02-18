/* Implementation of common interface functions for core allocators.
 *
 * Author: Sam Xi
 */

#include "base_allocator.h"

#include <map>
#include <string>

namespace xiosim {

BaseAllocator::BaseAllocator(int ncores) {
  num_cores = ncores;
}

BaseAllocator::~BaseAllocator() {
}

void BaseAllocator::DeallocateCoresForProcess(int asid) {
  if (core_allocs.find(asid) != core_allocs.end())
    core_allocs[asid] = 1;
}

int BaseAllocator::get_cores_for_asid(int asid) {
  if (core_allocs.find(asid) != core_allocs.end())
    return core_allocs[asid];
  return 0;
}

}  // namespace xiosim
