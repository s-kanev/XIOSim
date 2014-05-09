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
                                  SpeedupModelType model_type,
                                  double core_power,
                                  double uncore_power,
                                  int num_cores) {
                static GangAllocator instance(
                        opt_target, model_type, core_power, 
                        uncore_power, num_cores);
                return instance;
        }
        ~GangAllocator();
        int AllocateCoresForProcess(int asid, std::vector<double> scaling);

    private:
        GangAllocator(OptimizationTarget opt_target,
                      SpeedupModelType model_type,
                      double core_power,
                      double uncore_power,
                      int num_cores);
};

/* A credit-based core allocator. */
class PenaltyAllocator : public BaseAllocator {
    public:
        /* Gets a singleton allocator for a system with @ncores cores. */
        static PenaltyAllocator& Get(OptimizationTarget opt_target,
                                     SpeedupModelType model_type,
                                     double core_power,
                                     double uncore_power,
                                     int num_cores) {
            static PenaltyAllocator instance(
                    opt_target, model_type, core_power, 
                    uncore_power, num_cores);
            return instance;
        }
        int AllocateCoresForProcess(int asid, std::vector<double> scaling);
        // Returns the current penalty on process asid, or -1 if the process
        // does not exist in the allocator's knowledge.
        double get_penalty_for_asid(int asid);

    private:
        PenaltyAllocator(OptimizationTarget opt_target,
                         SpeedupModelType model_type,
                         double core_power,
                         double uncore_power,
                         int num_cores);
        std::map<int, double> *process_penalties;
        // Double precision floating point comparison accuracy.
        double SPEEDUP_EPSILON = 0.000001;
};

/* Locally optimal allocator that waits for all loops to align before making the
 * next allocation decision.
 */
class LocallyOptimalAllocator : public BaseAllocator {
    public:
        /* Gets a singleton allocator for a system with @ncores cores. */
        static LocallyOptimalAllocator& Get(
                OptimizationTarget opt_target,
                SpeedupModelType model_type,
                double core_power,
                double uncore_power,
                int num_cores) {
            static LocallyOptimalAllocator instance(
                    opt_target, model_type, core_power, 
                    uncore_power, num_cores);
            return instance;
        }
        ~LocallyOptimalAllocator();
        int AllocateCoresForProcess(int asid, std::vector<double> scaling);
    private:
        LocallyOptimalAllocator(
                OptimizationTarget opt_target,
                SpeedupModelType model_type,
                double core_power,
                double uncore_power,
                int num_cores);

        struct process_sync_t {
            int num_checked_in;
            int num_checked_out;
            bool allocation_complete;
        } process_sync;
        /* Process speedup for 1-n cores. */
        std::vector<std::vector<double>*> process_scaling;

        // Get/reset all class global variables for sharing among threads.
        void ResetState();
};

class AllocatorParser {
    public:
        static BaseAllocator& Get(std::string allocator_type,
                                  std::string allocator_opt_target,
                                  std::string speedup_model,
                                  double core_power,
                                  double uncore_power,
                                  int num_cores) {
            SpeedupModelType model_type;
            if (speedup_model == "linear")
                model_type = SpeedupModelType::LINEAR;
            else if (speedup_model == "logarithmic")
                model_type = SpeedupModelType::LOGARITHMIC;
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
                        opt_target, model_type, core_power,
                        uncore_power, max_cores);
            }
            if (allocator_type == "local")
                return LocallyOptimalAllocator::Get(
                        opt_target, model_type, core_power,
                        uncore_power, num_cores);
            if (allocator_type == "penalty")
                return PenaltyAllocator::Get(
                        opt_target, model_type, core_power,
                        uncore_power, num_cores);
            assert(false);
        }
};

}  // namespace xiosim

#endif
