#include <fstream>
#include <assert.h>
#include <string>
#include <vector>
#include <map>

#include <boost/tokenizer.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "../interface.h"

#include "parse_speedup.h"

// Cores for which speedup data is available.
static const int NUM_SPEEDUP_POINTS = 4;
static int interp_cores[NUM_SPEEDUP_POINTS] = {2, 4, 8, 16};
static std::map<std::string, double*> loop_speedup_map;

/* Linearly interpolates input speedup points. speedup_in is an array that
 * contains speedup values for 2, 4, 8, and 16 cores. speedup_out is an array
 * that has linearly interpolated values for 2-16 cores. The zeroth element of
 * speedup_out = 0, because there is no speedup with just one core.
 */
/* ========================================================================== */
static void InterpolateSpeedup(double* speedup_in, double* speedup_out)
{
    // Copy the existing data points.
    for (int i = 0; i < NUM_SPEEDUP_POINTS; i++)
        speedup_out[interp_cores[i]-1] = speedup_in[i];
    for (int i = 0; i < NUM_SPEEDUP_POINTS-1; i++) {
        double slope = (speedup_in[i+1]-speedup_in[i]) / (interp_cores[i+1]-interp_cores[i]);
        for (int j = interp_cores[i]+1; j < interp_cores[i+1]; j++) {  // Interpolate.
          speedup_out[j-1] = slope*(j - interp_cores[i+1]) + speedup_in[i+1];
        }
    }
}

/* Parses a comma separated value file that contains predicted speedups for
 * each loop when run on 2,4,8,and 16 cores and stores the data in a map.
 */
/* ========================================================================== */
void LoadHelixSpeedupModelData(const char* filepath)
{
    using std::string;
    using boost::tokenizer;
    using boost::escaped_list_separator;
    string line;
    std::ifstream speedup_loop_file(filepath);
    if (speedup_loop_file.is_open()) {
#ifdef PARSE_DEBUG
        std::cout << "Cores:\t\t";
        for (int j = 1; j <= MAX_CORES; j++)
            std::cout << j << "\t";
#endif
        std::cout << std::endl;
        while (getline(speedup_loop_file, line)) {
            // Ignore comments (lines starting with //).
            if (!boost::starts_with(line.c_str(), "//")) {
                tokenizer<escaped_list_separator<char>> tok(line);
                string loop_name;
                double* full_speedup_data = new double[MAX_CORES];
                int i = 1;  // Start at 1 so speedup for 1 core is 0.
                bool first_iteration = true;
                for (auto it = tok.begin(); it != tok.end(); ++it) {
                    if (first_iteration) {
                        loop_name = *it;
                        first_iteration = false;
                    } else {
                        assert(i < MAX_CORES);
                        full_speedup_data[i] = atof(it->c_str());
                        i++;
                    }
                }
                loop_speedup_map[loop_name] = full_speedup_data;
#ifdef PARSE_DEBUG
                std::cout << loop_name << " speedup:\t";
                for (int j = 0; j < MAX_CORES; j++)
                    std::cout << full_speedup_data[j] << "\t";
                std::cout << std::endl;
#endif
            }
        }
#ifdef PARSE_DEBUG
        std::cout << std::endl;
#endif
    } else {
        std::cerr << "Speedup file could not be opened.";
        exit(1);
    }
}

std::vector<double> GetHelixLoopScaling(const std::string &loop_name)
{
    double *res_raw = loop_speedup_map[loop_name];
    assert(res_raw != NULL);
    std::vector<double> res(MAX_CORES);
    for (int i=0; i < MAX_CORES; i++)
        res[i] = res_raw[i];
    return res;
}
