/* Implementation of BaseSpeedupModel functions common to all speedup models.
 * Author: Sam Xi.
 */

#include "base_speedup_model.h"

/* This is a helper convenience method that does some setup before calling the
 * function that does all the actual minimization work.
 */
double BaseSpeedupModel::MinimizeCoreAllocations(
        std::map<int, int> &core_alloc,
        std::vector<double> &process_scaling,
        std::vector<double> &process_serial_runtime,
        unsigned int opt_target) {

    std::vector<int> process_list;
    for (auto it = core_alloc.begin(); it != core_alloc.end(); ++it)
        process_list.push_back(it->first);

    metric_function_t MetricFunction;
    switch (opt_target) {
        case OptimizationTarget::ENERGY_TARGET:
            MetricFunction = &BaseSpeedupModel::ComputeEnergy;
            break;
        case OptimizationTarget::THROUGHPUT_TARGET:
            MetricFunction = &BaseSpeedupModel::ComputeThroughput;
            break;
    }

    return MinimizeCoreAllocations(
            core_alloc,
            process_scaling,
            process_serial_runtime,
            MetricFunction,
            process_list,
            -1);
}

/* Performs gradient descent to find the nearest minimum runtime point to a
 * starting core allocation, specified in core_alloc. Since the analytical
 * method should bring us very close to the true solution, it is a safe
 * assumption that any minima near this point is a global minima.
 */
double BaseSpeedupModel::MinimizeCoreAllocations(
        std::map<int, int> &core_alloc,
        std::vector<double> &process_scaling,
        std::vector<double> &process_serial_runtime,
        metric_function_t MetricFunction,
        std::vector<int> &process_list,
        double previous_max) {
    double current_min_metric = (this->*MetricFunction)(
            core_alloc, process_scaling, process_serial_runtime);
    if (process_list.empty())
        return current_min_metric;

    std::map<int, int> best_alloc(core_alloc);
    int num_processes = core_alloc.size();

    std::vector<int> valid_steps;
    valid_steps.push_back(1);
    valid_steps.push_back(0);
    valid_steps.push_back(-1);
    bool updated = false;
    do {
        updated = false;
        for (size_t i = 0; i < process_list.size(); i++) {
            for (auto j = valid_steps.begin(); j != valid_steps.end(); ++j) {
                // Test that the current allocation is at least one and not more
                // than num_cores - num_processes + 1 (which is the max per process
                // assuming each process is allocated at least one core). If not so,
                // then we ignore this allocation.
                int current_cores_alloc = ComputeCurrentCoresAllocated(best_alloc);
                std::map<int, int> temp_alloc(best_alloc);
                int current_proc = process_list[i];
                if (current_cores_alloc + *j <= num_cores &&
                    temp_alloc[current_proc] + *j > 0 &&
                    temp_alloc[current_proc] + *j <= num_cores - num_processes + 1) {
                    temp_alloc[current_proc] += *j;
                    std::vector<int> next_process_list(process_list);
                    next_process_list.erase(next_process_list.begin() + i);
                    double temp_energy = MinimizeCoreAllocations(
                        temp_alloc,
                        process_scaling,
                        process_serial_runtime,
                        MetricFunction,
                        next_process_list,
                        current_min_metric);
                    if (temp_energy < current_min_metric) {
                        current_min_metric = temp_energy;
                        best_alloc = temp_alloc;
                        updated = true;
                    }
                }
            }
        }
    } while (updated);

    core_alloc = best_alloc;
    return current_min_metric;
}

/* Calls the implemented ComputeRuntime() function in child classes to compute
 * energy.
 */
double BaseSpeedupModel::ComputeEnergy(
        std::map<int, int> &core_alloc,
        std::vector<double> &process_scaling,
        std::vector<double> &process_serial_runtime) {
    double current_energy = 0;
    double current_max_rt = 0;
    for (auto it = core_alloc.begin(); it != core_alloc.end(); ++it) {
        double runtime = ComputeRuntime(
                it->second,
                process_scaling[it->first],
                process_serial_runtime[it->first]);
        current_energy += core_power * runtime * it->second;
        if (runtime > current_max_rt)
            current_max_rt = runtime;
    }
    current_energy += uncore_power * current_max_rt;
    return current_energy;
}

/* Computes the sum of speedups based on the ComputeRuntime() function that is
 * implemented in child classes.
 */
double BaseSpeedupModel::ComputeThroughput(
        std::map<int, int> &core_alloc,
        std::vector<double> &process_scaling,
        std::vector<double> &process_serial_runtime) {
    double throughput = 0;
    for (auto it = core_alloc.begin(); it != core_alloc.end(); ++it) {
        double speedup = ComputeSpeedup(
                it->second,
                process_scaling[it->first],
                process_serial_runtime[it->first]);
        throughput += speedup;
    }
    return throughput;
}

/* Computes speedup for a single process based on the implemented
 * ComputeRuntime() function in child classes.
 */
double BaseSpeedupModel::ComputeSpeedup(
        int core_alloc, double process_scaling, double process_serial_runtime) {
    return process_serial_runtime /
           ComputeRuntime(core_alloc, process_scaling, process_serial_runtime);
}

/* Computes the total number of cores allocated in @core_alloc. */
int BaseSpeedupModel::ComputeCurrentCoresAllocated(
        std::map<int, int> &core_alloc) {
    int sum = 0;
    for (auto it = core_alloc.begin(); it != core_alloc.end(); ++it) {
        sum += it->second;
    }
    return sum;
}
