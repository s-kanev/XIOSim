/* Base class for scaling models.
 *
 * Specific scaling models will inherit from this class and implement the pure
 * virtual functions. They are free to define their own constructors and class
 * state.
 *
 * Author: Sam Xi
 */

#ifndef __BASE_SPEEDUP_MODEL__
#define __BASE_SPEEDUP_MODEL__

#include <map>
#include <vector>

/* Defines the different optimization targets. This is used to specify which
 * metric function should be used when computing the final minimal core
 * allocation.
 */
enum OptimizationTarget {
    ENERGY,
    THROUGHPUT
};

/* The different types of speedup models that can be created. */
enum SpeedupModelType {
    LINEAR,
    LOGARITHMIC
};

class BaseSpeedupModel {
    public:
        /* Sets the core power and uncore power values, which are needed to
         * compute energy.
         */
        BaseSpeedupModel(
                double core_power,
                double uncore_power,
                int num_cores,
                OptimizationTarget target = OptimizationTarget::THROUGHPUT) {
            this->core_power = core_power;
            this->uncore_power = uncore_power;
            this->num_cores = num_cores;
            opt_target = target;
        }

        virtual ~BaseSpeedupModel() {}

        /* Returns a core allocation that minimizes total energy consumption for
         * the system.
         *
         * Args:
         *   core_allocs: A map which will contain the number of cores allocated
         *     for process asid. This must be populated with all asids prior to
         *     calling the function (value does not matter).
         *   process_scaling: The fitted logarithmic scaling constants.
         *   process_serial_runtime: Serial runtimes for each process.
         *   num_cores: Total number of cores in the system.
         *
         * Returns:
         *   The optimal core allocation is stored in @core_allocs. There is no
         *   return value.
         */
        virtual void OptimizeEnergy(
                std::map<int, int> &core_allocs,
                std::vector<double> &process_scaling,
                std::vector<double> &process_serial_runtime) = 0;

        /* Returns a core allocation that maximizes sum of speedups for the
         * system.
         *
         * Args and return values:
         *   See OptimizeEnergy().
         */
        virtual void OptimizeThroughput(
                std::map<int, int> &core_allocs,
                std::vector<double> &process_scaling,
                std::vector<double> &process_serial_runtime) = 0;

        /* Compute the scaling factor that fits the scaling data for the
         * implemented speedup model.
         *
         * Args:
         *   process_scaling: A vector that holds speedups on 1,2,4,8, and 16
         *     cores for a process.
         *
         * Returns:
         *   The scaling factor for the implemented speedup model.
         */
        virtual double ComputeScalingFactor(
                std::vector<double> &process_scaling) = 0;

        /* Returns the maximum scaling factor possible under the implemented
         * speedup model. Maximum scaling means as close to 1x speedup per
         * additional core as possible, and it does not account for superlinear
         * speedup effects.
         */
        virtual double ComputeIdealScalingFactor() = 0;

        /* A convenience function that calls the appropriate optimization
         * function based on the opt_target member of the class. This is useful
         * when the optimization function is dynamically set at runtime, or it
         * would be required to modify the code and recompile.
         *
         * For all arguments and returns, see OptimizeEnergy().
         */
        void OptimizeForTarget(
                std::map<int, int> &core_allocs,
                std::vector<double> &process_scaling,
                std::vector<double> &process_serial_runtime);

    protected:
        /* Two scaling factors within this value are considered equal. */
        const double SCALING_EPSILON = 0.000001;

        /* The power consumed by a core. Assume that core power is constant
         * regardless of utilization.
         */
        double core_power;

        /* The power consumed by uncore units, such as shared last-level caches,
         * memory controllers, power management systems, and IO.
         */
        double uncore_power;

        /* The total number of cores in the system. */
        int num_cores;

        /* The target function for which to optimize core allocations. */
        OptimizationTarget opt_target;

        /* Computes parallel runtime of a process given the number of allocated
         * cores and scaling data.
         *
         * Args:
         *   num_cores_alloc: The number of cores allocated to this process.
         *   process_scaling: The scaling factor under the current speedup
         *     model.
         *   process_serial_runtime: The serial runtime (on one core) of this
         *     process.
         *
         * Returns:
         *   The parallel runtime under the current speedup model.
         */
        virtual double ComputeRuntime(
                int num_cores_alloc,
                double process_scaling,
                double process_serial_runtime) = 0;

        /* Computes parallel speedup under a core allocation and scaling
         * behavior. Calls ComputeRuntime().
         *
         * Args:
         *   See ComputeRuntime().
         *
         * Returns:
         *   The parallel speedup under the current speedup model.
         */
        double ComputeSpeedup(int core_alloc,
                              double process_scaling,
                              double process_serial_runtime);

        /* Performs gradient descent to find the nearest minimum runtime point
         * to a starting core allocation, specified in core_alloc. Since the
         * analytical method is guaranteed to bring us very close to the true
         * solution, it is generally a safe assumption that any minima near
         * this point is a global minima.
         *
         * Args:
         *   opt_target: A value of the OptimizationTarget enum indicating which
         *     optimization metric function should be used for the minimization.
         *   For all other arguments, see OptimizeEnergy().
         *
         * Returns: The minimum energy of the system, with the corresponding
         *   minimum core allocation stored in @core_alloc.
         */
        double MinimizeCoreAllocations(
                std::map<int, int> &core_alloc,
                std::vector<double> &process_scaling,
                std::vector<double> &process_serial_runtime,
                OptimizationTarget opt_target);

        /* Returns the current number of cores allocated in @core_alloc.
         *
         * Args:
         *   core_alloc: Map from asid to number of cores allocated for that
         *     process.
         *
         * Returns:
         *   The total number of cores allocated.
         */
        int ComputeCurrentCoresAllocated(std::map<int, int> &core_alloc);

        /* Computes the energy a system would consume under a given core
         * allocation and process scaling behaviors.
         *
         * Args:
         *   See OptimizeEnergy().
         *
         * Returns:
         *   The energy consumed by the system.
         */
        double ComputeEnergy(
                std::map<int, int> &core_alloc,
                std::vector<double> &process_scaling,
                std::vector<double> &process_serial_runtime);

        /* Computes the reciprocal of the sum of speedups for a system under a
         * given core allocation and process scaling behaviors. The reciprocal
         * is required because the gradient descent function searches for a
         * minimum, and optimization for throughput searches for LARGER values
         * of throughput.
         *
         * Args:
         *   See OptimizeEnergy().
         *
         * Returns:
         *   The reciprocal sum of speedups of the system.
         */
        double ComputeThroughputMetric(
                std::map<int, int> &core_alloc,
                std::vector<double> &process_scaling,
                std::vector<double> &process_serial_runtime);

    private:
        /* Signature for a function that defines the metric of a particular
         * optimization target, such as a function that computes total system
         * energy or throughput.
         */
        typedef double (BaseSpeedupModel::*metric_function_t)(
                std::map<int, int>&,
                std::vector<double>&,
                std::vector<double>&);

        /* Performs gradient descent recursively. This is the function that
         * does the true work; the other overloaded function is a convenience
         * method.
         *
         * Args:
         *   MetricFunction: A member function pointer to the metric function
         *     for an optimization target.
         *   process_list: A vector of asids. Each recursion of this function
         *     explores adding, removing, or keeping constant a core to a
         *     process. When a process's allocation is tentatively modified,
         *     that process's asid is removed from this list. The function
         *     returns when this list is empty, thus guaranteeing convergence.
         *   previous_max: The worst-case runtime from the previous invocation
         *     of this function. This is used because this function is
         *     recursive. This equals -1 if the current worst-case runtime is
         *     worse than the previous maximum (which means the current
         *     allocation is less optimal). The first invocation of this
         *     function sets this parameter to -1.
         *   For all other arguments, see OptimizeEnergy().
         *
         * Returns:
         *   The minimum energy point found from the provided starting point.
         *   The allocation corresponding to this minimum energy is stored in
         *   @core_alloc.
         */
        double MinimizeCoreAllocations(
                std::map<int, int> &core_alloc,
                std::vector<double> &process_scaling,
                std::vector<double> &process_serial_runtime,
                metric_function_t MetricFunction,
                std::vector<int> &process_list,
                double previous_max);
};

#endif
