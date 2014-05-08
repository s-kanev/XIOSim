/* Implementation of the linear speedup model functions.
 *
 * Author: Sam Xi.
 */

#include <cmath>
#include <map>
#include <vector>

#include "speedup_models.h"

void LinearSpeedupModel::OptimizeEnergy(
        std::map<int, int> &core_allocs,
        std::vector<double> &process_scaling,
        std::vector<double> &process_serial_runtime) {
    std::vector<double> gammas;
    double gamma_sum = 0;
    for (auto it = core_allocs.begin(); it != core_allocs.end(); ++it) {
        int asid = it->first;
        gammas.push_back(
                process_serial_runtime[asid] / process_scaling[asid]);
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
                // If this process is allocated zero cores, it automatically
                // becomes the maximum runtime process (infinite runtime).
                max_runtime_asid = it->first;
                break;
            }
        }
        core_allocs[max_runtime_asid]++;
        cores_remaining--;
    }
    // Now find the actual global runtime minimum around this starting point.
    MinimizeCoreAllocations(
            core_allocs,
            process_scaling,
            process_serial_runtime);
}

double LinearSpeedupModel::ComputeRuntime(int num_cores_alloc,
                      double process_scaling,
                      double process_serial_runtime) {
    return (process_serial_runtime / (process_scaling * num_cores_alloc));
}
