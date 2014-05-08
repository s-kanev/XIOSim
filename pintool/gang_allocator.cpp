#include "allocators_impl.h"

namespace xiosim {

GangAllocator::GangAllocator(
        OptimizationTarget opt_target,
        SpeedupModelType model_type,
        double core_power,
        double uncore_power,
        int num_cores) : BaseAllocator(
                opt_target, model_type, core_power, uncore_power, num_cores) {}

GangAllocator::~GangAllocator() {}

int GangAllocator::AllocateCoresForProcess(
        int asid, std::vector<double> scaling)
{
    (void) scaling; (void) asid;

    lk_lock(&allocator_lock, 1);
    core_allocs[asid] = num_cores;
    lk_unlock(&allocator_lock);

    UpdateSHMAllocation(asid, num_cores);
    return num_cores;
}

} //namespace xiosim
