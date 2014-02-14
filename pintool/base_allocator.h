/* A core allocator interface.
 *
 * Author: Sam Xi.
 */

#ifndef __ALLOCATOR_H__
#define __ALLOCATOR_H__

#include <map>
#include <string>
#include "../synchronization.h"

namespace xiosim {

struct pid_cores_info {
  double current_penalty = 0;
  int num_cores_allocated = 0;
};

const int ERR_LOOP_NOT_FOUND = -1;

class BaseAllocator {
  protected:
    const double MARGINAL_SPEEDUP_THRESHOLD = 0.4;
    std::map<int, pid_cores_info> *process_info_map;
    std::map<std::string, double*> *loop_speedup_map;
    int num_cores;
    XIOSIM_LOCK allocator_lock;

  public:
    /* Constructs a new allocator for a system with ncores cores. */
    BaseAllocator(int ncores);

    /* Deletes all state for this allocator. */
    virtual ~BaseAllocator();

    /* Allocates cores for a loop named loop_name based on its scaling behavior
     * and current penalties held and returns the number of cores allotted in
     * num_cores_alloc. If the process is already allocated cores, this function 
     * implicitly releases all the previous allocated cores and returns a new 
     * allocation.
     * Returns:
     *   0 if allocation is successful.
     *   ERR_LOOP_NOT_FOUND if the loop is not found.
     */
    virtual int AllocateCoresForLoop(
        std::string loop_name, int asid, int* num_cores_alloc) = 0;

    /* Releases all cores allocated for pid except for 1, so that the process
     * can continue to execute serial code on a single thread. If the pid does
     * not exist, nothing happens.
     */
    void DeallocateCoresForProcess(int asid);

    /* Parses a comma separated value file that contains predicted speedups for
     * each loop when run on 2,4,8,and 16 cores and stores the data in a map.
     */
    void LoadHelixSpeedupModelData(char* filepath);

    /* Interpolate speedup data points based on the 4 data points given in the
     * input files.
     */
    void InterpolateSpeedup(double* speedup_in, double* speedup_out);

    /* Returns a copy of the process data for pid in data. If pid does not exist
     * in the map, the data pointer is not modified. */
    void get_data_for_asid(int asid, pid_cores_info* data);
};

}  // namespace xiosim

#endif
