#ifndef __PARSE_SPEEDUP_H__
#define __PARSE_SPEEDUP_H__
/* Related to reading IR model speedup predictions. */

#include <vector>
#include <string>

// Describes loop scaling behavior in terms of its scaling across cores and its
// serial runtime.
struct loop_data {
    double* speedup;
    double serial_runtime;  // This is an ESTIMATE.
    double serial_runtime_variance;
};

/* Parses a comma separated value file that contains predicted speedups for
 * each loop when run on 2, 4, 8, and 16 cores, as well as the mean serial
 * runtime and variance over all invocations. Stores this data in a map.
 */
void LoadHelixSpeedupModelData(const std::string filepath);

/* Returns parsed scaling data for a cerain loop. */
std::vector<double> GetHelixLoopScaling(const std::string& loop_name);

/* Returns the full loop scaling attributes of a loop: core scaling, serial
 * runtime, and serial runtime variance across invocations.
 */
loop_data* GetHelixFullLoopData(const std::string& loop_name);

#endif /* __PARSE_SPEEDUP_H__ */
