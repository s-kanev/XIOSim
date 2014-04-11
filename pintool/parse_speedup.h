#ifndef __PARSE_SPEEDUP_H__
#define __PARSE_SPEEDUP_H__
/* Related to reading IR model speedup predictions. */

#include <vector>
#include <string>

#include "linreg.h"

// Describes loop scaling behavior in terms of its scaling across cores and its
// serial runtime.
struct loop_data {
  double *speedup;
  double serial_runtime;  // This is an ESTIMATE.
  double serial_runtime_variance;
};

/* Parses a comma separated value file that contains predicted speedups for
 * each loop when run on 2, 4, 8, and 16 cores, as well as the mean serial
 * runtime and variance over all invocations. Stores this data in a map.
 */
void LoadHelixSpeedupModelData(const char* filepath);

/* Returns parsed scaling data for a cerain loop. */
std::vector<double> GetHelixLoopScaling(const std::string &loop_name);

/* Returns the full loop scaling attributes of a loop: core scaling, serial
 * runtime, and serial runtime variance across invocations.
 */
loop_data* GetHelixFullLoopData(const std::string &loop_name);

/* Runs linear regression on the speedup data for a simple linear speedup model.
 *
 * Args:
 *   speedup: An array of size num_cores.
 *   num_cores: The number of total cores in the system.
 * Returns (via the parameters):
 *   slope: A double pointer with the slope of the fitted line.
 *   intercept: A double pointer with the intercept of the fitted line.
 */
static void PerformLinearRegression(
    double* speedup, int num_cores, double* slope, double* intercept);

#endif /* __PARSE_SPEEDUP_H__ */
