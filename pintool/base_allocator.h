/* A core allocator interface.
 *
 * Author: Sam Xi.
 */

#ifndef __ALLOCATOR_H__
#define __ALLOCATOR_H__

#include <map>
#include <vector>
#include <string>
#include "synchronization.h"

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
    virtual int
    AllocateCoresForProcess(int asid, std::vector<double> scaling, double serial_runtime) = 0;

    /* Releases all cores allocated for pid except for 1, so that the
     * process can continue to execute serial code on a single thread. If
     * the pid does not exist, nothing happens.
     */
    void DeallocateCoresForProcess(int asid);

    /* Returns the number of cores allocated to process asid. If asid does
     * not exist in the map, returns 0. */
    int get_cores_for_asid(int asid);

    BaseSpeedupModel* speedup_model;

    /* Resets the core allocation tracker. Children can override this
     * function to reset additional variables. */
    virtual void ResetState();

    /* Returns a list of processes to unblock after the most recent
     * allocation call by the specified process.
     *
     * Args:
     *   asid: The asid of the process that called AllocateCoresForProcess()
     *      and needs to unblock the blocking IPC messages.
     *
     * Returns:
     *   A vector of asids that this process should unblock. If the asid is
     *   not found, the returned vector is empty.
     */
    std::vector<int> get_processes_to_unblock(int asid);

  protected:
    const double MARGINAL_SPEEDUP_THRESHOLD = 0.4;
    std::map<int, int> core_allocs;
    std::map<int, std::vector<int>> processes_to_unblock;
    int num_cores;
    XIOSIM_LOCK allocator_lock;
    BaseAllocator(OptimizationTarget target,
                  SpeedupModelType speedup_model_type,
                  double core_power,
                  double uncore_power,
                  int ncores);
    void UpdateSHMAllocation(int asid, int allocated_cores) const;

  private:
    BaseAllocator(){};
    BaseAllocator(BaseAllocator const&);
    void operator=(BaseAllocator const&);
};

}  // namespace xiosim

#endif
