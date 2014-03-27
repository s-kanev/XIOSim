/* Different core allocation functions for different optimization targets.
 * Currently, we support optimizing for throughput and energy.
 *
 * Author: Sam Xi
 */

namespace xiosim {

/* Searches for the core allocation that provides the most system throughput
 * across n processes.
 *
 * Arguments:
 *   core_allocs: Map of address space ids (asids), or process identifiers, to
 *     the number of cores it is allocated.
 *   scaling: Vector of doubles containing scaling data vs. cores.
 *   num_cores: total number of cores in the system.
 */
void OptimizeThroughput(std::map<int, int> &core_allocs, 
                        std::vector<std::vector<double>*> process_scaling, 
                        int num_cores);

}  // namespace xiosim
