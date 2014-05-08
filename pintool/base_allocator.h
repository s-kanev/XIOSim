/* A core allocator interface.
 *
 * Author: Sam Xi.
 */

#ifndef __ALLOCATOR_H__
#define __ALLOCATOR_H__

#include <map>
#include <vector>
#include <string>
#include "../synchronization.h"

#include "base_speedup_model.h"

namespace xiosim {

class BaseAllocator {
    public:
        /* Deletes all state for this allocator. */
        virtual ~BaseAllocator();

        /* Allocates cores based on prior scaling behavior and current penalties
         * held and returns the number of cores allotted in num_cores_alloc. If
         * the process is already allocated cores, this function implicitly
         * releases all the previous allocated cores and returns a new
         * allocation.
         * Returns: number of allocated cores.
         */
        virtual int AllocateCoresForProcess(int asid,
                std::vector<double> scaling) = 0;

        /* Releases all cores allocated for pid except for 1, so that the
         * process can continue to execute serial code on a single thread. If
         * the pid does not exist, nothing happens.
         */
        void DeallocateCoresForProcess(int asid);

        /* Returns the number of cores allocated to process asid. If asid does
         * not exist in the map, returns 0. */
        int get_cores_for_asid(int asid);

        BaseSpeedupModel *speedup_model;

    protected:
        const double MARGINAL_SPEEDUP_THRESHOLD = 0.4;
        std::map<int, int> core_allocs;
        int num_cores;
        XIOSIM_LOCK allocator_lock;
        BaseAllocator(OptimizationTarget target,
                      SpeedupModelType model_type,
                      double core_power,
                      double uncore_power,
                      int ncores);
        void UpdateSHMAllocation(int asid, int allocated_cores) const;

    private:
        BaseAllocator() {};
        BaseAllocator(BaseAllocator const&);
        void operator=(BaseAllocator const&);
};

}  // namespace xiosim

#endif
