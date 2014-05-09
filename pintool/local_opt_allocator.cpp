/* Implementation of the locally optimal core allocation policy.
 *
 * At time T = 0, N processes will be allocated cores. When each process
 * completes, it will release the cores allocated. The next allocation cannot
 * occur until all N processes have completed and released their cores.
 *
 * Author: Sam Xi
 */

#include "boost_interprocess.h"
#include "../interface.h"
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

LocallyOptimalAllocator::LocallyOptimalAllocator(
        OptimizationTarget opt_target,
        SpeedupModelType model_type,
        double core_power,
        double uncore_power,
        int num_cores) : BaseAllocator(
                opt_target, model_type, core_power, uncore_power, num_cores),
        process_scaling() {
    ResetState();
}

LocallyOptimalAllocator::~LocallyOptimalAllocator() {
}

void LocallyOptimalAllocator::ResetState() {
    process_sync.num_checked_in = 0;
    process_sync.num_checked_out = 0;
    process_sync.allocation_complete = false;
    process_scaling.resize(*num_processes);
}

int LocallyOptimalAllocator::AllocateCoresForProcess(
        int asid, std::vector<double> scaling) {
    lk_lock(&allocator_lock, 1);

    // Start each process with 1 core allocated.
    core_allocs[asid] = 1;
    process_scaling[asid] = &scaling;
    process_sync.num_checked_in++;
    // Wait for all processes in the system to check in before proceeding.
    while (process_sync.num_checked_in < *num_processes) {
        lk_unlock(&allocator_lock);
        usleep(10000);
        lk_lock(&allocator_lock, 1);
    }
    assert(process_sync.num_checked_in == *num_processes);

    // Only the first thread that reaches this point needs to perform the
    // allocation optimization function. All other threads can wait for this to
    // complete and then simply use the output.
    if (!process_sync.allocation_complete) {
        OptimizeThroughput(core_allocs, process_scaling, num_cores);
        process_sync.allocation_complete = true;
    }

    int allocated_cores = core_allocs[asid]++;
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

}    // namespace xiosim
