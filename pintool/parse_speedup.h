#ifndef __PARSE_SPEEDUP_H__
#define __PARSE_SPEEDUP_H__
/* Related to reading IR model speedup predictions. */

#include <vector>
#include <string>

/* Parses a comma separated value file that contains predicted speedups for
 * each loop when run on 2,4,8,and 16 cores and stores the data in a map.
 */
void LoadHelixSpeedupModelData(const char* filepath);

/* Returns parsed scaling data for a cerain loop. */
std::vector<double> GetHelixLoopScaling(const std::string &loop_name);

#endif /* __PARSE_SPEEDUP_H__ */
