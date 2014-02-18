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

namespace xiosim {

LocallyOptimalAllocator::LocallyOptimalAllocator(int num_cores) :
  BaseAllocator(num_cores), process_scaling() {
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
    // Search for optimal core allocation.
    int total_cores_alloc = *num_processes;
    int optimum_found = false;
    while (!optimum_found) {
      double max_speedup = 0;
      int asid_with_max_speedup = -1;
      for (auto alloc_pair : core_allocs) {
        int curr_asid = alloc_pair.first;
        int curr_core_alloc = alloc_pair.second;
        double curr_speedup = process_scaling[curr_asid]->at(curr_core_alloc);
        if (curr_speedup > max_speedup && total_cores_alloc < num_cores) {
          max_speedup = curr_speedup;
          asid_with_max_speedup = curr_asid;
        }
      }
      if (max_speedup > 0) {
        core_allocs[asid_with_max_speedup]++;
        total_cores_alloc++;
      }
      if (max_speedup < 0 || total_cores_alloc == num_cores ) {
        // We have not decided how to deal with non-monotonic speedup curves, so
        // for now we stop if all processes start exhibiting negative speedup.
        optimum_found = true;
      }
    }
    process_sync.allocation_complete = true;
  }

  int allocated_cores = core_allocs[asid]++;
  process_sync.num_checked_out++;
  if (process_sync.num_checked_out == *num_processes) {
    // The last thread executing this code will reset class variables for the
    // next allocation.
    ResetState();
  }
  lk_unlock(&allocator_lock);
  UpdateSHMAllocation(asid, allocated_cores);
  return allocated_cores;
}

}  // namespace xiosim
