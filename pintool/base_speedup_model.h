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

class BaseSpeedupModel {
    public:
        /* Sets the core power and uncore power values, which are needed to
         * compute energy.
         */
        BaseSpeedupModel(double core_power, double uncore_power, int num_cores) {
            this->core_power = core_power;
            this->uncore_power = uncore_power;
            this->num_cores = num_cores;
        }

        /* Returns a core allocation that minimizes total energy consumption for
         * the system.
         *
         * Args:
         *   core_allocs: A reference to a map which will contain the number of
         *     cores allocated for process asid. This must be populated with all
         *     asids prior to calling the function (value does not matter).
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

    protected:
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

        /* Performs gradient descent to find the nearest minimum runtime point
         * to a starting core allocation, specified in core_alloc. Since the
         * analytical method is guaranteed to bring us very close to the true
         * solution, it is generally a safe assumption that any minima near
         * this point is a global minima. 
         *
         * Args:
         *   core_alloc: The core allocation from where to start gradient
         *     descent. It is a map from asid to the number of cores currently
         *     allocated.  When the function returns, the updated core
         *     allocations are stored in this map.
         *   process_scaling: A vector of linear scaling
         *     factors, where the index of the element equals the asid of the
         *     process.
         *   process_serial_runtime: A vector of serial runtimes for
         *     each process, where the index of the element equals the asid of
         *     the process.
         *
         * Returns: The minimum energy of the system, with the corresponding
         *   minimum core allocation stored in @core_alloc.
         */
        double MinimizeCoreAllocations(
                std::map<int, int> &core_alloc,
                std::vector<double> &process_scaling,
                std::vector<double> &process_serial_runtime);

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
         *   process_scaling: A vector of linear scaling
         *     factors, where the index of the element equals the asid of the
         *     process.
         *   process_serial_runtime: A vector of serial runtimes for
         *     each process, where the index of the element equals the asid of
         *     the process.
         *
         * Returns:
         *   The energy consumed by the system.
         */
        double ComputeEnergy(
                std::map<int, int> &core_alloc,
                std::vector<double> &process_scaling,
                std::vector<double> &process_serial_runtime);

    private:
        /* Performs gradient descent recursively. This is the function that
         * does the true work; the other overloaded function is a convenience
         * method.
         *
         * Additional args:
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
                std::vector<int> &process_list,
                double previous_max);

};

#endif
