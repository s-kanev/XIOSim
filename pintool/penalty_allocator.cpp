/* Implementation of the penalty core allocation policy.
 *
 * Author: Sam Xi
 */

#include <cmath>
#include <iostream>
#include <map>
#include <string>

#include "allocators_impl.h"
#include "base_speedup_model.h"

namespace xiosim {

PenaltyAllocator::PenaltyAllocator(
        OptimizationTarget opt_target,
        SpeedupModelType speedup_model,
        double core_power,
        double uncore_power,
        int num_cores) : BaseAllocator(
                opt_target, speedup_model, core_power, uncore_power, num_cores) {
    process_penalties = new std::map<int, double>();
}

// Use this method only for debugging purposes, as it does not exist in the base
// class.
double PenaltyAllocator::get_penalty_for_asid(int asid) {
    if (process_penalties->find(asid) != process_penalties->end())
        return process_penalties->operator[](asid);
    return -1;
}

int PenaltyAllocator::AllocateCoresForProcess(
        int asid, std::vector<double> scaling, double serial_runtime) {
    // The nth element of @scaling is the incremental amount of speedup attained
    // if running under that many cores.
#ifdef DEBUG
    std::cout << "Allocating cores to process " << asid << "." << std::endl;
#endif
    int allocated_cores;

    // Add entry for this process if it doesn't exist.
    if (core_allocs.find(asid) == core_allocs.end()) {
        core_allocs[asid] = 1;
        process_penalties->operator[](asid) = 0;
    }

    // Compute total cores available and optimal cores for this loop.
    int available_cores = num_cores;
    int optimal_cores = 0;
    for (auto it = core_allocs.begin(); it != core_allocs.end(); ++it) {
        // Assume that the current process is giving up its cores.
        if (it->first != asid)
            available_cores -= it->second;
    }
    for (int i = 1; i < num_cores; i++) {
        // Comparing if scaling[i] >= threshold, but direct comparison of
        // doubles is inexact, so the greater than comparison needs to be
        // separated from the equals comparison.
        if (scaling[i] > MARGINAL_SPEEDUP_THRESHOLD ||
            std::abs(scaling[i] - MARGINAL_SPEEDUP_THRESHOLD) <= SPEEDUP_EPSILON)
            optimal_cores = i+1;
        else
            break;
    }

    // Implementation of the penalty policy.
    if (optimal_cores <= available_cores) {
        int reduced_alloc_cores = optimal_cores;
        // Pay any penalty this process currently holds.
        double* current_penalty =
                &(process_penalties->operator[](asid));
#ifdef DEBUG
        std::cout << "Process " << asid << " currently has penalty of "
                  << *current_penalty << "." << std::endl;
#endif
        if (*current_penalty > 0) {
            double speedup_lost = 1;
            while ((optimal_cores - reduced_alloc_cores) * (speedup_lost - 1) <=
                         *current_penalty) {
                speedup_lost *= 1 + scaling[reduced_alloc_cores - 1];
                reduced_alloc_cores--;
                if (reduced_alloc_cores == 0) {
                    reduced_alloc_cores = 1;
                    break;
                }
            }
            // Speedup is a multiplicative factor, so subtract 100%.
            speedup_lost -= 1;
            double penalty_paid = (
                optimal_cores - reduced_alloc_cores) * speedup_lost;
            *current_penalty -= penalty_paid;
#ifdef DEBUG
            std::cout << "Process " << asid << " paid penalty of "
                      << penalty_paid << std::endl;
#endif
        }
        allocated_cores = reduced_alloc_cores;
    } else {
        // Assess penalty to the other process.
        double speedup_lost = 1;
        for (int i = available_cores + 1; i <= optimal_cores; i++)
            speedup_lost *= (1 + scaling[i - 1]);
        speedup_lost -= 1;
        // Divide the penalty by the number of processes in the system - 1. This
        // is a simple generalization of this policy for more than two
        // processes.
        double penalty_per_pid = (
            speedup_lost * (optimal_cores - available_cores))/
            (core_allocs.size() - 1);
        for (auto it = process_penalties->begin();
                 it != process_penalties->end(); ++it) {
            if (it->first != asid) {
             (it->second) += penalty_per_pid;
#ifdef DEBUG
                std::cout << "Assessed penalty " << penalty_per_pid
                          << " to process " << it->first << std::endl;
#endif
            }
        }
        allocated_cores = available_cores;
    }

    core_allocs[asid] = allocated_cores;
#ifdef DEBUG
    std::cout << "Process " << asid << " requested " << optimal_cores <<
            " cores, was allocated " << allocated_cores << " cores." <<
            std::endl << std::endl;
#endif
    UpdateSHMAllocation(asid, allocated_cores);
    return allocated_cores;
}


}    // namespace xiosim
