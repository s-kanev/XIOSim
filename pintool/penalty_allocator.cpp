/* Implementation of the penalty core allocation policy.
 *
 * Author: Sam Xi
 */

#include <iostream>
#include <map>

#include "allocators_impl.h"

namespace xiosim {

void PenaltyAllocator::AllocateCoresForLoop(
    char* loop_name, pid_t pid, int* num_cores_alloc) {
  // loop_speedup_map maps loop names to arrays of size n, where n is the number
  // of cores. The nth element is the incremental amount of speedup attained if
  // running under that many cores.
#ifdef DEBUG
  std::cout << "Allocating cores to process " << pid << "." << std::endl;
#endif
  std::string loop(loop_name);
  auto loop_it = loop_speedup_map->find(loop);
  double* speedup_per_core;
  if (loop_it != loop_speedup_map->end()) {
    speedup_per_core = loop_it->second;
  } else {
    std::cout << "Loop not found." << std::endl;
    *num_cores_alloc = -1;
    return;  // Loop not found, return...
  }

  // Add entry for this process if it doesn't exist.
  if (process_info_map->find(pid) == process_info_map->end()) {
    pid_cores_info data;
    process_info_map->operator[](pid) = data;
  }

  // Compute total cores available and optimal cores for this loop.
  int available_cores = num_cores;
  int optimal_cores = 0;
  for (auto it = process_info_map->begin();
       it != process_info_map->end(); ++it) {
    if (it->first != pid)
      available_cores -= it->second.num_cores_allocated;
  }
  for (int i = 1; i < num_cores; i++) {
    if (speedup_per_core[i] >= MARGINAL_SPEEDUP_THRESHOLD)
      optimal_cores = i+1;
    else
      break;
  }

  // Implementation of the penalty policy.
  if (optimal_cores <= available_cores) {
    int reduced_alloc_cores = optimal_cores;
    // Pay any penalty rhis process currently holds.
    double* current_penalty =
        &(process_info_map->operator[](pid).current_penalty);
#ifdef DEBUG
    std::cout << "Process " << pid << " currently has penalty of " <<
      *current_penalty << "." << std::endl;
#endif
    if (*current_penalty > 0) {
      double speedup_lost = 1;
      while ((optimal_cores - reduced_alloc_cores) * (speedup_lost - 1) <=
             *current_penalty) {
        speedup_lost *= 1 + speedup_per_core[reduced_alloc_cores - 1];
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
      std::cout << "Process " << pid << " paid penalty of " << penalty_paid <<
          std::endl;
#endif
    }
    *num_cores_alloc = reduced_alloc_cores;
  } else {
    // Assess penalty to the other process.
    double speedup_lost = 1;
    for (int i = available_cores + 1; i <= optimal_cores; i++)
      speedup_lost *= (1 + speedup_per_core[i - 1]);
    speedup_lost -= 1;
    // Divide the penalty by the number of processes in the system - 1. This is
    // a simple generalization of this policy for more than two processes.
    double penalty_per_pid = (speedup_lost * (optimal_cores - available_cores))/
        (process_info_map->size() - 1);
    for (auto it = process_info_map->begin();
         it != process_info_map->end(); ++it) {
      if (it->first != pid) {
       (it->second.current_penalty) += penalty_per_pid;
#ifdef DEBUG
        std::cout << "Assessed penalty " << penalty_per_pid << " to process "
            << it->first << std::endl;
#endif
      }
    }
    *num_cores_alloc = available_cores;
  }

  process_info_map->operator[](pid).num_cores_allocated = *num_cores_alloc;
#ifdef DEBUG
  std::cout << "Process " << pid << " requested " << optimal_cores <<
      " cores, was allocated " << *num_cores_alloc << " cores." <<
      std::endl << std::endl;
#endif
}


}  // namespace xiosim
