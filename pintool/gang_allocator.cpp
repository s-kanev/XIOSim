#include "allocators_impl.h"

namespace xiosim {

GangAllocator::GangAllocator(
        OptimizationTarget opt_target,
        SpeedupModelType speedup_model,
        double core_power,
        double uncore_power,
        int num_cores) : BaseAllocator(
                opt_target, speedup_model, core_power, uncore_power, num_cores)
{}

GangAllocator::~GangAllocator() {}

int GangAllocator::AllocateCoresForProcess(
        int asid, std::vector<double> scaling, double serial_runtime)
{
    (void) scaling; (void) asid;

    lk_lock(&allocator_lock, 1);
    core_allocs[asid] = num_cores;
    std::vector<int> unblock_list;
    unblock_list.push_back(asid);
    processes_to_unblock[asid] = unblock_list;
    lk_unlock(&allocator_lock);

    UpdateSHMAllocation(asid, num_cores);
    return num_cores;
}

} //namespace xiosim
