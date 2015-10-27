/* Implementation of the locally optimal core allocation policy.
 *
 * At time T = 0, N processes will be allocated cores. When each process
 * completes, it will release the cores allocated. The next allocation cannot
 * occur until all N processes have completed and released their cores.
 *
 * Author: Sam Xi
 */

#include "../synchronization.h"
#include "multiprocess_shared.h"
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <unistd.h>

#include "allocators_impl.h"
#include "optimization_targets.h"

namespace xiosim {

LocallyOptimalAllocator::LocallyOptimalAllocator(OptimizationTarget opt_target,
                                                 SpeedupModelType speedup_model,
                                                 double core_power,
                                                 double uncore_power,
                                                 int num_cores)
    : BaseAllocator(opt_target, speedup_model, core_power, uncore_power, num_cores)
    , process_scaling()
    , process_serial_runtime() {
    ResetState();
}

LocallyOptimalAllocator::~LocallyOptimalAllocator() {}

/* Resets the checked-in process tracker and scaling data.
 * Note: do not clear processes_to_unblock, because this structure can be called
 * at any time after an allocation request to get the list of processes to
 * unblock, and ResetState() is called when every allocation finishes.
 */
void LocallyOptimalAllocator::ResetState() {
    BaseAllocator::ResetState();
    process_sync.num_checked_in = 0;
    process_sync.num_checked_out = 0;
    process_sync.allocation_complete = false;
    process_scaling.clear();
    process_scaling.resize(*num_processes);
    process_serial_runtime.clear();
    process_serial_runtime.resize(*num_processes);
}

int LocallyOptimalAllocator::AllocateCoresForProcess(int asid,
                                                     std::vector<double> scaling,
                                                     double serial_runtime) {
    lk_lock(&allocator_lock, 1);

    // On the first call for this process, initialize some parameters.
    if (core_allocs.find(asid) == core_allocs.end()) {
        core_allocs[asid] = 1;
        process_scaling[asid] = speedup_model->ComputeScalingFactor(scaling);
        process_serial_runtime[asid] = serial_runtime;
        process_sync.num_checked_in++;
    }
    // Wait for all processes in the system to check in before proceeding. If
    // not all processes have checked in, return -1.
    if (process_sync.num_checked_in < *num_processes) {
        // Erase this entry since this thread was not the last to check in.
        processes_to_unblock.erase(asid);
        lk_unlock(&allocator_lock);
        return -1;
    }
    assert(process_sync.num_checked_in == *num_processes);

    // Only the first thread that reaches this point needs to perform the
    // allocation optimization function. All other threads can wait for this to
    // complete and then simply use the output.
    if (!process_sync.allocation_complete) {
        speedup_model->OptimizeForTarget(core_allocs, process_scaling, process_serial_runtime);
        process_sync.allocation_complete = true;
        // Add all asids in core_allocs into the unblocking list.
        // std::vector<int> unblock_asids;
        processes_to_unblock[asid] = std::vector<int>();
        for (auto it = core_allocs.begin(); it != core_allocs.end(); ++it)
            processes_to_unblock[asid].push_back(it->first);
    }

    int allocated_cores = core_allocs[asid];
    process_sync.num_checked_out++;
    if (process_sync.num_checked_out == *num_processes) {
        // The last thread executing this code will reset class variables for
        // the next allocation.
        ResetState();
    }
    lk_unlock(&allocator_lock);
    UpdateSHMAllocation(asid, allocated_cores);
    return allocated_cores;
}

}  // namespace xiosim
