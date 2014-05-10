/* Implementation of the linear speedup model functions.
 *
 * Author: Sam Xi.
 */

#include <cmath>
#include <cstring>
#include <map>
#include <vector>

#include "linreg.h"
#include "speedup_models.h"

/* Optimizes energy for the linear speedup model. The solution is:
 *   n_i = N * C_i / sum(C_j for all j).
 * where N is the total number of cores and C_i is the linear scaling factor for
 * the ith process.
 */
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
            process_serial_runtime,
            OptimizationTarget::ENERGY);
}

/* Returns a core allocation that maximizes sum of speedups across all
 * processes. Under linear scaling assumptions, such an allocation gives the
 * best scaling process all cores in the system. However, we enforce the
 * constraint that each process is allocated at least one core; thus, the best
 * scaling process would allocated less than the total number of cores.
 *
 * When several processes scale equally, cores can be distributed in any manner
 * between them. For simplicity, we distribute them as evenly as possible.
 */
void LinearSpeedupModel::OptimizeThroughput(
        std::map<int, int> &core_allocs,
        std::vector<double> &process_scaling,
        std::vector<double> &process_serial_runtime) {
    if (process_scaling.empty() ||
        process_serial_runtime.empty())
        return;

    double max_scaling = process_scaling.at(0);
    std::vector<int> best_procs;
    best_procs.push_back(0);
    for (size_t i = 1; i < process_scaling.size(); i++) {
        if (process_scaling.at(i) > max_scaling) {
            max_scaling = process_scaling.at(i);
            best_procs.clear();
            best_procs.push_back(i);
        } else if (std::abs(process_scaling.at(i) - max_scaling) <=
                   SCALING_EPSILON) {
            // We've found a process with scaling equal to the current best.
            best_procs.push_back(i);
        }
    }

    // Assign cores.
    for (size_t i = 0; i < process_scaling.size(); i++)
        core_allocs[i] = 1;
    int remaining_cores = num_cores - core_allocs.size() + best_procs.size();
    for (size_t i = 0; i < best_procs.size(); i++) {
        int cores_per_proc = int(
                (double)remaining_cores/(best_procs.size() - i));
        core_allocs[best_procs.at(i)] = cores_per_proc;
        remaining_cores -= cores_per_proc;
    }
}

/* Computes runtime under the linear model. */
double LinearSpeedupModel::ComputeRuntime(int num_cores_alloc,
                      double process_scaling,
                      double process_serial_runtime) {
    return (process_serial_runtime / (process_scaling * num_cores_alloc));
}

/* Runs linear regression on the speedup data for the linear speedup model. */
double LinearSpeedupModel::ComputeScalingFactor(
        std::vector<double> &core_scaling) {
    LinearRegressionIntercept lr;
    for (size_t i = 0; i <= core_scaling.size(); i++) {
        // Subtract 1 because the regression equation is y = 1 + bx, and we want
        // y = bx, which we can get with a simple change of variable y' = y-1.
        Point2D p(i+1, core_scaling[i]-1);
        lr.addPoint(p);
    }
    return lr.getB();
}
