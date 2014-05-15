#ifndef __ALLOCATORS_IMPL_H__
#define __ALLOCATORS_IMPL_H__

#include <string>
#include <vector>
#include <assert.h>

#include "base_allocator.h"
#include "base_speedup_model.h"

namespace xiosim {

/* Trivial allocator that always allocates X cores. */
class GangAllocator : public BaseAllocator {
    public:
        /* Gets a singleton allocator for a system with @ncores cores. */
        static GangAllocator& Get(OptimizationTarget opt_target,
                                  SpeedupModelType speedup_model_type,
                                  double core_power,
                                  double uncore_power,
                                  int num_cores) {
                static GangAllocator instance(
                        opt_target, speedup_model_type, core_power,
                        uncore_power, num_cores);
                return instance;
        }
        ~GangAllocator();
        int AllocateCoresForProcess(
                int asid, std::vector<double> scaling, double serial_runtime);

    private:
        GangAllocator(OptimizationTarget opt_target,
                      SpeedupModelType speedup_model_type,
                      double core_power,
                      double uncore_power,
                      int num_cores);
};

/* A credit-based core allocator. */
class PenaltyAllocator : public BaseAllocator {
    public:
        /* Gets a singleton allocator for a system with @ncores cores. */
        static PenaltyAllocator& Get(OptimizationTarget opt_target,
                                     SpeedupModelType speedup_model_type,
                                     double core_power,
                                     double uncore_power,
                                     int num_cores) {
            static PenaltyAllocator instance(
                    opt_target, speedup_model_type, core_power,
                    uncore_power, num_cores);
            return instance;
        }
        int AllocateCoresForProcess(
                int asid, std::vector<double> scaling, double serial_runtime);
        // Returns the current penalty on process asid, or -1 if the process
        // does not exist in the allocator's knowledge.
        double get_penalty_for_asid(int asid);

        /* Clear all stored state. */
        void ResetState();

    private:
        PenaltyAllocator(OptimizationTarget opt_target,
                         SpeedupModelType speedup_model_type,
                         double core_power,
                         double uncore_power,
                         int num_cores);
        /* On the first time a parallel loop begins, the scaling data of other
         * programs is unknown. We assume that all other programs scale equally
         * with the current program and initialize the allocator state as such.
         *
         * Args:
         *   current_scaling_factor: The scaling factor of the current process.
         *   current_serial_runtime: The serial runtime of the current process.
         */
        void FirstAllocationInit(
                double current_scaling_factor, double current_serial_runtiem);
        /* Double precision floating point comparison accuracy. */
        double SPEEDUP_EPSILON = 0.000001;
        /* Penalties for each process. */
        std::map<int, double> *process_penalties;
        /* Scaling factors for each process. */
        std::vector<double> process_scaling;
        /* Serial runtimes for each process. */
        std::vector<double> process_serial_runtime;
};

/* Locally optimal allocator that waits for all loops to align before making the
 * next allocation decision.
 */
class LocallyOptimalAllocator : public BaseAllocator {
    public:
        /* Gets a singleton allocator for a system with @ncores cores. */
        static LocallyOptimalAllocator& Get(
                OptimizationTarget opt_target,
                SpeedupModelType speedup_model_type,
                double core_power,
                double uncore_power,
                int num_cores) {
            static LocallyOptimalAllocator instance(
                    opt_target, speedup_model_type, core_power,
                    uncore_power, num_cores);
            return instance;
        }
        ~LocallyOptimalAllocator();

        // Computes the optimal core allocation for an optimization target and
        // returns the number of cores allocated for process asid. This function
        // waits for all parallel loops to align before making a decision. If
        // all processes have not arrived at a loop boundary, this function
        // returns -1.
        int AllocateCoresForProcess(
                int asid, std::vector<double> scaling, double serial_runtime);

        // Get/reset all class global variables for sharing among threads.
        void ResetState();
    private:
        LocallyOptimalAllocator(
                OptimizationTarget opt_target,
                SpeedupModelType speedup_model_type,
                double core_power,
                double uncore_power,
                int num_cores);

        struct process_sync_t {
            int num_checked_in;
            int num_checked_out;
            bool allocation_complete;
        } process_sync;
        /* Scaling factors for each process. */
        std::vector<double> process_scaling;
        /* Serial runtimes for each process. */
        std::vector<double> process_serial_runtime;
};

class AllocatorParser {
    public:
        static BaseAllocator& Get(std::string allocator_type,
                                  std::string allocator_opt_target,
                                  std::string speedup_model_str,
                                  double core_power,
                                  double uncore_power,
                                  int num_cores) {
            SpeedupModelType speedup_model_type;
            if (speedup_model_str == "linear")
                speedup_model_type = SpeedupModelType::LINEAR;
            else if (speedup_model_str == "logarithmic")
                speedup_model_type = SpeedupModelType::LOGARITHMIC;
            else
                assert(false);

            OptimizationTarget opt_target;
            if (allocator_opt_target == "energy")
                opt_target = OptimizationTarget::ENERGY;
            else if (allocator_opt_target == "throughput")
                opt_target = OptimizationTarget::THROUGHPUT;
            else
                assert(false);

            if (allocator_type.find("gang") != allocator_type.npos) {
                char name[16];
                int max_cores;
                if (sscanf(allocator_type.c_str(),
                           "%[^:]:%d",
                           name,
                           &max_cores) != 2)
                        assert(false);
                assert(max_cores > 0 && max_cores <= num_cores);
                return GangAllocator::Get(
                        opt_target, speedup_model_type, core_power,
                        uncore_power, max_cores);
            }
            if (allocator_type == "local")
                return LocallyOptimalAllocator::Get(
                        opt_target, speedup_model_type, core_power,
                        uncore_power, num_cores);
            if (allocator_type == "penalty")
                return PenaltyAllocator::Get(
                        opt_target, speedup_model_type, core_power,
                        uncore_power, num_cores);
            assert(false);
        }
};

}  // namespace xiosim

#endif
