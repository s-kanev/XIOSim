/* Speedup model based on the logarithmic function 1 + C log[n].
 * Author: Sam Xi
 */
#ifndef __SPEEDUP_MODELS_IMPL__
#define __SPEEDUP_MODELS_IMPL__

#include <map>
#include <vector>

#include "base_speedup_model.h"

class LinearSpeedupModel : public BaseSpeedupModel {
    public:
        LinearSpeedupModel(double core_power, double uncore_power) :
            BaseSpeedupModel(core_power, uncore_power) {}
        void OptimizeEnergy(
                std::map<int, int> &core_allocs,
                std::vector<double> &process_scaling,
                std::vector<double> &process_serial_runtime,
                int num_cores);
        double ComputeRuntime(
                int num_cores_alloc,
                double process_scaling,
                double process_serial_runtime);
};

class LogSpeedupModel : public BaseSpeedupModel {
    public:
        LogSpeedupModel(double core_power, double uncore_power) :
            BaseSpeedupModel(core_power, uncore_power) {}
        void OptimizeEnergy(
                std::map<int, int> &core_allocs,
                std::vector<double> &process_scaling,
                std::vector<double> &process_serial_runtime,
                int num_cores);
        double ComputeRuntime(
                int num_cores_alloc,
                double process_scaling,
                double process_serial_runtime);
    private:
        /* Lambert W function.
         * Was ~/C/LambertW.c written K M Briggs Keith dot Briggs at bt dot com
         * 97 May 21.
         * Revised KMB 97 Nov 20; 98 Feb 11, Nov 24, Dec 28; 99 Jan 13; 00 Feb
         * 23; 01 Apr 09.
        */
        double LambertW(const double z);
};

#endif
