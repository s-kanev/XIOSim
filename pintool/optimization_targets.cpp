/* Implementations of core allocation functions for different optimization
 * targets. These functions are currently naive, brute force implementations.
 * More efficient solutions may be implemented later.
 *
 * Author: Sam Xi
 */

#include "boost_interprocess.h"
#include "../interface.h"
#include "../synchronization.h"
#include "multiprocess_shared.h"
#include <map>
#include <vector>
#include <string>

#include "optimization_targets.h"

namespace xiosim {

/* Optimizes the core allocation for greatest system throughput across n
 * processes. This assumes that speedup is monotically increasing. This is
 * generally a safe assumption as we do not use SMT.
 */
void OptimizeThroughput(std::map<int, int> &core_allocs, 
                        std::vector<std::vector<double>*> process_scaling, 
                        int num_cores) {
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
}

}  // namespace xiosim
