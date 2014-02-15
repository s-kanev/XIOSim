#include "allocators_impl.h"

namespace xiosim {

GangAllocator::GangAllocator(int ncores) : BaseAllocator(ncores)
{

}

GangAllocator::~GangAllocator()
{

}

int GangAllocator::AllocateCoresForLoop(std::string loop_name, int asid, int* num_cores_alloc)
{
  (void) loop_name; (void) asid;

  lk_lock(&allocator_lock, 1);
  core_allocs->at(asid) = num_cores;
  *num_cores_alloc = num_cores;
  lk_unlock(&allocator_lock);
  return 0;
}

} //namespace xiosim
