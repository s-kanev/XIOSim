/* Header for all speedup model classes.
 * Author: Sam Xi
 */

#ifndef __SPEEDUP_MODELS__
#define __SPEEDUP_MODELS__

#include <map>
#include <vector>

#include "base_speedup_model.h"

/* Two scaling factors within this value are considered equal. */
const double SCALING_EPSILON = 0.000001;

class LinearSpeedupModel : public BaseSpeedupModel {
    public:
        LinearSpeedupModel(
                double core_power, double uncore_power, int num_cores) :
            BaseSpeedupModel(core_power, uncore_power, num_cores) {}

        /* Implements energy optimization under linear scaling assumptions. */
        void OptimizeEnergy(
                std::map<int, int> &core_allocs,
                std::vector<double> &process_scaling,
                std::vector<double> &process_serial_runtime);

        /* Implements throughput optimization, where throughput is defined as
         * the sum of speedup. */
        void OptimizeThroughput(
                std::map<int, int> &core_allocs,
                std::vector<double> &process_scaling,
                std::vector<double> &process_serial_runtime);

        /* Computes runtime as serial_runtime / (scaling * ncores). */
        double ComputeRuntime(
                int num_cores_alloc,
                double process_scaling,
                double process_serial_runtime);
};

class LogSpeedupModel : public BaseSpeedupModel {
    public:
        LogSpeedupModel(double core_power, double uncore_power, int num_cores) :
            BaseSpeedupModel(core_power, uncore_power, num_cores) {}

        /* Implements energy optimization under logarithmic scaling assumptions.
         */
        void OptimizeEnergy(
                std::map<int, int> &core_allocs,
                std::vector<double> &process_scaling,
                std::vector<double> &process_serial_runtime);

        /* Implements throughput optimization, where throughput is defined as
         * the sum of speedup. */
        void OptimizeThroughput(
                std::map<int, int> &core_allocs,
                std::vector<double> &process_scaling,
                std::vector<double> &process_serial_runtime);

        /* Computes runtime as serial_runtime / (1 + scaling * ln (ncores)). */
        double ComputeRuntime(
                int num_cores_alloc,
                double process_scaling,
                double process_serial_runtime);

        /* Lambert W function.
         * Was ~/C/LambertW.c written K M Briggs Keith dot Briggs at bt dot com
         * 97 May 21.
         * Revised KMB 97 Nov 20; 98 Feb 11, Nov 24, Dec 28; 99 Jan 13; 00 Feb
         * 23; 01 Apr 09.
        */
        double LambertW(const double z);
};

#endif
