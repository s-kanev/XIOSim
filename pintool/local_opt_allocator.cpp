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
#include <string>
#include <unistd.h>

#include "allocators_impl.h"

namespace xiosim {

LocallyOptimalAllocator::LocallyOptimalAllocator(int num_cores) :
  BaseAllocator(num_cores) {
  process_alloc_map = new std::map<int, loop_alloc_pair>();
  ResetState();
}

LocallyOptimalAllocator::~LocallyOptimalAllocator() {
  delete process_alloc_map;
}

void LocallyOptimalAllocator::ResetState() {
  process_sync.num_checked_in = 0;
  process_sync.num_checked_out = 0;
  process_sync.allocation_complete = false;
  process_alloc_map->clear();
}

int LocallyOptimalAllocator::AllocateCoresForLoop(
    std::string loop_name, int asid, int* num_cores_alloc) {
  lk_lock(&allocator_lock, 1);
  if (loop_speedup_map->find(loop_name) == loop_speedup_map->end()) {
    lk_unlock(&allocator_lock);
    return ERR_LOOP_NOT_FOUND;
  }

  // Start each loop with 1 core allocated.
  loop_alloc_pair pair(loop_name, 1);
  process_alloc_map->operator[](asid) = pair;
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
      for (auto it = process_alloc_map->begin();
           it != process_alloc_map->end(); ++it) {
        int curr_asid = it->first;
        std::string curr_loop = it->second.first;
        int curr_core_alloc = it->second.second;
        double curr_speedup =
            loop_speedup_map->operator[](curr_loop)[curr_core_alloc];
        if (curr_speedup > max_speedup && total_cores_alloc < num_cores) {
          max_speedup = curr_speedup;
          asid_with_max_speedup = curr_asid;
        }
      }
      if (max_speedup > 0) {
        process_alloc_map->operator[](asid_with_max_speedup).second++;
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

  *num_cores_alloc = process_alloc_map->operator[](asid).second++;
  core_allocs->operator[](asid) = *num_cores_alloc;
  process_sync.num_checked_out++;
  if (process_sync.num_checked_out == *num_processes) {
    // The last thread executing this code will reset class variables for the
    // next allocation.
    ResetState();
  }
  lk_unlock(&allocator_lock);
  return 0;
}

}  // namespace xiosim
