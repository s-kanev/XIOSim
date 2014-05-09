/* Implementation of common interface functions for core allocators.
 *
 * Author: Sam Xi
 */

#include <map>
#include <string>

#include "boost_interprocess.h"
#include "../synchronization.h"
#include "../interface.h"
#include "multiprocess_shared.h"

#include "base_allocator.h"
#include "speedup_models.h"

namespace xiosim {

BaseAllocator::BaseAllocator(
        OptimizationTarget target,
        SpeedupModelType speedup_model_type,
        double core_power,
        double uncore_power,
        int ncores) {
    num_cores = ncores;
    switch (speedup_model_type) {
        case SpeedupModelType::LINEAR:
            speedup_model = new LinearSpeedupModel(
                    core_power, uncore_power, num_cores, target);
            break;
        case SpeedupModelType::LOGARITHMIC:
            speedup_model = new LogSpeedupModel(
                    core_power, uncore_power, num_cores, target);
            break;
    }
}

BaseAllocator::~BaseAllocator() {
    delete speedup_model;
}

void BaseAllocator::DeallocateCoresForProcess(int asid) {
    lk_lock(&allocator_lock, 1);
    if (core_allocs.find(asid) != core_allocs.end())
        core_allocs[asid] = 1;
    lk_unlock(&allocator_lock);
}

int BaseAllocator::get_cores_for_asid(int asid) {
    int res = 0;
    lk_lock(&allocator_lock, 1);
    if (core_allocs.find(asid) != core_allocs.end())
        res = core_allocs[asid];
    lk_unlock(&allocator_lock);
    return res;
}

void BaseAllocator::UpdateSHMAllocation(int asid, int allocated_cores) const {
    UpdateProcessCoreAllocation(asid, allocated_cores);
}

}    // namespace xiosim
