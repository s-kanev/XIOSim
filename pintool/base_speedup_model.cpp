#include "base_speedup_model.h"

/* Performs gradient descent to find the nearest minimum runtime point to a
 * starting core allocation, specified in core_alloc. Since the analytical
 * method is guaranteed to bring us very close to the true solution, it is a
 * safe assumption that any minima near this point is a global minima. This has
 * been verified with extensive testing.
 */
double BaseSpeedupModel::MinimizeCoreAllocations(
        std::map<int, int> &core_alloc,
        std::vector<double> &process_scaling,
        std::vector<double> &process_serial_runtime,
        double prev_max) {
    double current_max = 0;
    // Compute the current worst case runtime.
    for (auto it = core_alloc.begin(); it != core_alloc.end(); ++it) {
        double runtime = ComputeRuntime(
                it->second,
                process_scaling[it->first],
                process_serial_runtime[it->first]);
        if (runtime > current_max)
            current_max = runtime;
    }
    // If the current max runtime is larger than the previous maximum (from the
    // calling function), then this allocation is less optimal.
    if (prev_max != -1 && current_max >= prev_max)
        return -1;

    // This allocation is more optimal than the previous one. Continue.
    // TODO: This might not work for log scaling where the total number of
    // allocated cores can change.
    std::map<int, int> best_alloc(core_alloc);
    for (auto i = core_alloc.begin(); i != core_alloc.end(); ++i) {
        core_alloc[i->first] ++;
        for (auto j = core_alloc.begin(); j != core_alloc.end(); ++j) {
            if (i->first != j->first && j->second > 1) {
                core_alloc[j->first] --;
                double next_max = MinimizeCoreAllocations(
                        core_alloc,
                        process_scaling,
                        process_serial_runtime,
                        current_max);
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
