#include <cmath>
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
static std::map<std::string, loop_data*> loop_data_map;

/* This function converts absolute speedups on x cores into marginal speedup.
 * HELIX speedup data is given in absolute terms, but it is easier for the
 * allocators to work with marginal speedups.
 */
/* ========================================================================== */
static void ConvertToMarginalSpeedup(double* speedup, int num_points) {
    double temp1 = speedup[0], temp2 = 0;
    for (int i = 1; i < num_points; i++) {
        temp2 = speedup[i];
        speedup[i] -= temp1;
        temp1 = temp2;
    }
}

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
        double slope = (speedup_in[i+1]-speedup_in[i]) /
            (interp_cores[i+1]-interp_cores[i]);
        // Interpolate.
        for (int j = interp_cores[i]+1; j < interp_cores[i+1]; j++) {
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
                double *speedup_data = new double[NUM_SPEEDUP_POINTS];
                double serial_runtime = 0;
                double serial_runtime_variance = 0;
                int i = 0;
                bool first_iteration = true;
                for (auto it = tok.begin(); it != tok.end(); ++it) {
                    if (first_iteration) {
                        loop_name = *it;
                        first_iteration = false;
                    } else if (i < NUM_SPEEDUP_POINTS) {
                        // Speedup data points.
                        speedup_data[i] = atof(it->c_str());
                        i++;
                    } else {
                        // Serial runtime and variance.
                        serial_runtime = atof(it->c_str());
                        it++;
                        serial_runtime_variance = atof(it->c_str());
                    }
                }
                loop_data *data = new loop_data();
                data->speedup = speedup_data;
                data->serial_runtime = serial_runtime;
                data->serial_runtime_variance = serial_runtime_variance;
                loop_data_map[loop_name] = data;
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

// Returns the core scaling attributes of a loop.
std::vector<double> GetHelixLoopScaling(const std::string &loop_name)
{
    double *res_raw = loop_data_map[loop_name]->speedup;
    assert(res_raw != NULL);
    std::vector<double> res(NUM_SPEEDUP_POINTS+1);
    res[0] = 1;  // for 1 core, speedup is 1.
    for (size_t i=0; i < NUM_SPEEDUP_POINTS; i++)
        res[i+1] = res_raw[i];
    return res;
}

// Returns the full loop scaling attributes of a loop
loop_data* GetHelixFullLoopData(const std::string &loop_name) {
  return loop_data_map[loop_name];
}
