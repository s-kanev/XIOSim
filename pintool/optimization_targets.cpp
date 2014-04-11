/* Implementations of core allocation functions for different optimization 
 * targets. Some functions are currently naive, brute force implementations.
 * More efficient solutions may be implemented later.
 *
 * Author: Sam Xi
 */

#include "boost_interprocess.h"
#include "../interface.h"
#include "../synchronization.h"
#include "multiprocess_shared.h"
#include <cmath>
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

/* Performs gradient descent to find the nearest minimum runtime point to a
 * starting core allocation, specified in core_alloc. Since the analytical
 * method is guaranteed to bring us very close to the true solution, it is a
 * safe assumption that any minima near this point is a global minima. This has
 * been verified with extensive testing.
 * TODO: Since this method is not needed outside of this file, we may want to
 * consider a refactoring that makes this a private member of some class.
 */
double MinimizeCoreAllocations(std::map<int, int> &core_alloc,
                               std::vector<double> &gammas,
                               double prev_max) {
  double current_max = 0;
  // Compute the current worst case runtime.
  for (auto it = core_alloc.begin(); it != core_alloc.end(); ++it) {
    double runtime = gammas[it->first]/it->second;
    if (runtime > current_max)
      current_max = runtime;
  }
  // If the current max runtime is larger than the previous maximum (from the
  // calling function), then this allocation is less optimal.
  if (prev_max != -1 && current_max >= prev_max)
    return -1;

  // This allocation is more optimal than the previous one. Continue.
  std::map<int, int> best_alloc(core_alloc);
  for (auto i = core_alloc.begin(); i != core_alloc.end(); ++i) {
    core_alloc[i->first] ++;
    for (auto j = core_alloc.begin(); j != core_alloc.end(); ++j) {
      if (i->first != j->first && j->second > 1) {
        core_alloc[j->first] --;
        double next_max = MinimizeCoreAllocations(core_alloc, gammas, current_max);
        if (next_max != -1 && next_max < current_max) {
          best_alloc = core_alloc;
          current_max = next_max;
        } else {
          core_alloc[j->first] ++;
        }
      }
    }
    core_alloc[i->first] --;
  }
  core_alloc = best_alloc;
  return current_max;
}

/* Return a core allocation that optimizes total running energy. This assumes
 * linear scaling and constant core and uncore power.
 * TODO: Consider making core_allocs also a vector.
 */
void OptimizeEnergyForLinearScaling(
    std::map<int, int> &core_allocs,
    std::vector<double> &process_linear_scaling,
    std::vector<double> &process_serial_runtime,
    int num_cores) {
  std::vector<double> gammas;
  double gamma_sum = 0;
  for (auto it = core_allocs.begin(); it != core_allocs.end(); ++it) {
    int asid = it->first;
    gammas.push_back(
        process_serial_runtime[asid] / process_linear_scaling[asid]);
    gamma_sum += gammas[asid];
  }
  // Compute continous core allocations.
  int total_cores_alloc = 0;
  for (int asid = 0; asid < (int)gammas.size(); asid++) {
    int curr_alloc = floor(num_cores * (gammas[asid] / gamma_sum));
    total_cores_alloc += curr_alloc;
    core_allocs[asid] = curr_alloc;
  }
  // If chip is currently underutilized, allocate the remaining cores.
  int cores_remaining = num_cores - total_cores_alloc;
  while (cores_remaining > 0) {
    int max_runtime_asid = 0;
    double max_runtime = 0;
    for (auto it = core_allocs.begin(); it != core_allocs.end(); ++it) {
      if (it->second != 0) {
        double runtime = gammas[it->first]/it->second;
        if (runtime > max_runtime) {
          max_runtime_asid = it->first;
          max_runtime = runtime;
        }
      } else {
        // If this process is allocated zero cores, it automatically becomes the
        // maximum runtime process (infinite runtime).
        max_runtime_asid = it->first;
        break;
      }
    }
    core_allocs[max_runtime_asid]++;
    cores_remaining--;
  }
  // Now find the actual global runtime minimum around this starting point.
  MinimizeCoreAllocations(core_allocs, gammas, -1);
}

}  // namespace xiosim
