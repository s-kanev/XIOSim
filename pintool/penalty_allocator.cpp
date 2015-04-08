/* Implementation of the penalty core allocation policy.
 *
 * Author: Sam Xi
 */

#include <cmath>
#include <iostream>
#include <map>
#include <string>

#include "multiprocess_shared.h"
#include "allocators_impl.h"
#include "base_speedup_model.h"

namespace xiosim {

PenaltyAllocator::PenaltyAllocator(OptimizationTarget opt_target,
                                   SpeedupModelType speedup_model,
                                   double core_power,
                                   double uncore_power,
                                   int num_cores)
    : BaseAllocator(opt_target, speedup_model, core_power, uncore_power, num_cores)
    , process_scaling()
    , process_serial_runtime() {
    process_penalties = new std::map<int, double>();
    process_scaling.resize(*num_processes);
    process_serial_runtime.resize(*num_processes);
    processes_to_unblock.clear();
}

/* Resets all penalty and scaling data.
 * Note: processes_to_unblock CAN be cleared here because this method is not
 * called after every allocation request.
 */
void PenaltyAllocator::ResetState() {
    BaseAllocator::ResetState();
    process_scaling.clear();
    process_scaling.resize(*num_processes);
    process_serial_runtime.clear();
    process_serial_runtime.resize(*num_processes);
    process_penalties->clear();
    processes_to_unblock.clear();
}

/* On the first time a parallel loop begins, the scaling data of other programs
 * is unknown. We assume that all other programs scale equally with the current
 * program and initialize the allocator's state as such.
 */
void PenaltyAllocator::FirstAllocationInit(double current_scaling_factor,
                                           double current_serial_runtime) {
    // If any seg fault happens in these loops, then there's obviously something
    // wrong, since all the sizes of the containers must be equal.
    for (size_t i = 0; i < process_scaling.size(); i++) {
        process_scaling[i] = current_scaling_factor;
        process_serial_runtime[i] = current_serial_runtime;
        core_allocs[i] = 1;
        process_penalties->operator[](i) = 0;
    }
}

// Use this method only for debugging purposes, as it does not exist in the base
// class.
double PenaltyAllocator::get_penalty_for_asid(int asid) {
    if (process_penalties->find(asid) != process_penalties->end())
        return process_penalties->operator[](asid);
    return 0;
}

int PenaltyAllocator::AllocateCoresForProcess(int asid,
                                              std::vector<double> scaling,
                                              double serial_runtime) {
    lk_lock(&allocator_lock, 1);
// The nth element of @scaling is the incremental amount of speedup attained
// if running under that many cores.
#ifdef DEBUG
    std::cout << "Allocating cores to process " << asid << "." << std::endl;
#endif
    int allocated_cores;

    // Initialize allocator state when the first process requests cores.
    double scaling_factor = speedup_model->ComputeScalingFactor(scaling);
    if (core_allocs.empty()) {
        FirstAllocationInit(scaling_factor, serial_runtime);
    } else {
        process_scaling[asid] = scaling_factor;
        process_serial_runtime[asid] = serial_runtime;
    }

    std::map<int, int> temp_core_allocs(core_allocs);
    speedup_model->OptimizeForTarget(temp_core_allocs, process_scaling, process_serial_runtime);
    // Copy the allocation results from temp_core_allocs.
    core_allocs[asid] = temp_core_allocs[asid];
    for (auto it = temp_core_allocs.begin(); it != temp_core_allocs.end(); ++it)
        std::cout << "Process " << it->first << " :" << it->second << std::endl;

    // Compute total cores available and optimal cores for this loop.
    int optimal_cores = core_allocs[asid];
    int available_cores = num_cores;
    for (auto it = core_allocs.begin(); it != core_allocs.end(); ++it) {
        // Assume that the current process is giving up its cores.
        if (it->first != asid)
            available_cores -= it->second;
    }

    // Implementation of the penalty policy.
    if (optimal_cores <= available_cores) {
        int reduced_alloc_cores = optimal_cores;
        // Pay any penalty this process currently holds.
        double* current_penalty = &(process_penalties->operator[](asid));
#ifdef DEBUG
        std::cout << "Process " << asid << " currently has penalty of " << *current_penalty << "."
                  << std::endl;
#endif
        if (*current_penalty > 0) {
            double speedup_lost = 1;
            while ((optimal_cores - reduced_alloc_cores) * (speedup_lost - 1) <= *current_penalty) {
                speedup_lost *= speedup_model->ComputeSpeedup(reduced_alloc_cores, scaling_factor);
                reduced_alloc_cores--;
                if (reduced_alloc_cores == 0) {
                    reduced_alloc_cores = 1;
                    break;
                }
            }
            // Speedup is a multiplicative factor, so subtract 100%.
            speedup_lost -= 1;
            double penalty_paid = (optimal_cores - reduced_alloc_cores) * speedup_lost;
            *current_penalty -= penalty_paid;
#ifdef DEBUG
            std::cout << "Process " << asid << " paid penalty of " << penalty_paid << std::endl;
#endif
        }
        allocated_cores = reduced_alloc_cores;
    } else {
        // Assess penalty to the other process.
        double speedup_lost = 1;
        for (int i = available_cores + 1; i <= optimal_cores; i++)
            speedup_lost *= speedup_model->ComputeSpeedup(i, scaling_factor);
        speedup_lost -= 1;
        // Divide the penalty by the number of processes in the system - 1. This
        // is a simple generalization of this policy for more than two
        // processes.
        double penalty_per_pid =
            (speedup_lost * (optimal_cores - available_cores)) / (core_allocs.size() - 1);
        for (auto it = process_penalties->begin(); it != process_penalties->end(); ++it) {
            if (it->first != asid) {
                (it->second) += penalty_per_pid;
#ifdef DEBUG
                std::cout << "Assessed penalty " << penalty_per_pid << " to process " << it->first
                          << std::endl;
#endif
            }
        }
        allocated_cores = available_cores;
    }

    core_allocs[asid] = allocated_cores;
#ifdef DEBUG
    std::cout << "Process " << asid << " requested " << optimal_cores << " cores, was allocated "
              << allocated_cores << " cores." << std::endl << std::endl;
#endif
    std::vector<int> unblock_list;
    unblock_list.push_back(asid);
    processes_to_unblock[asid] = unblock_list;
    lk_unlock(&allocator_lock);
    UpdateSHMAllocation(asid, allocated_cores);
    return allocated_cores;
}

}  // namespace xiosim
