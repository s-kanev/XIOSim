/* Implementation of common interface functions for core allocators.
 *
 * Author: Sam Xi
 */

#include "boost_interprocess.h"
#include "base_allocator.h"

#include "assert.h"
#include <fstream>
#include <iostream>
#include <map>
#include <string>

namespace xiosim {

// Cores for which speedup data is available.
const int NUM_SPEEDUP_POINTS = 4;
int cores[NUM_SPEEDUP_POINTS] = {2, 4, 8, 16};

BaseAllocator::BaseAllocator(int ncores) {
  core_allocs = new std::map<int, int>();
  loop_speedup_map = new std::map<std::string, double*>();
  num_cores = ncores;
}

BaseAllocator::~BaseAllocator() {
  for (auto it = loop_speedup_map->begin(); it != loop_speedup_map->end(); ++it)
    delete[] it->second;
  delete loop_speedup_map;
  delete core_allocs;
}

void BaseAllocator::DeallocateCoresForProcess(int asid) {
  if (core_allocs->find(asid) != core_allocs->end())
    core_allocs->operator[](asid) = 1;
}

void BaseAllocator::LoadHelixSpeedupModelData(char* filepath) {
  using std::string;
  using boost::tokenizer;
  using boost::escaped_list_separator;
  string line;
  std::ifstream speedup_loop_file(filepath);
  if (speedup_loop_file.is_open()) {
#ifdef DEBUG
    std::cout << "Cores:\t\t";
    for (int j = 1; j <= num_cores; j++)
      std::cout << j << "\t";
#endif
    std::cout << std::endl;
    while (getline(speedup_loop_file, line)) {
      // Ignore comments (lines starting with //).
      if (!boost::starts_with(line.c_str(), "//")) {
        tokenizer<escaped_list_separator<char>> tok(line);
        string loop_name;
        double partial_speedup_data[NUM_SPEEDUP_POINTS];
        double* full_speedup_data = new double[num_cores];
        int i = 0;
        bool first_iteration = true;
        for (auto it = tok.begin(); it != tok.end(); ++it) {
          if (first_iteration) {
            loop_name = *it;
            first_iteration = false;
          } else {
            assert(i < NUM_SPEEDUP_POINTS);
            partial_speedup_data[i] = atof(it->c_str());
            i++;
          }
        }
        InterpolateSpeedup(partial_speedup_data, full_speedup_data);
        loop_speedup_map->operator[](loop_name) = full_speedup_data;
#ifdef DEBUG
        std::cout << loop_name << " speedup:\t";
        for (int j = 0; j < num_cores; j++)
          std::cout << full_speedup_data[j] << "\t";
        std::cout << std::endl;
#endif
      }
    }
#ifdef DEBUG
    std::cout << std::endl;
#endif
  } else {
    std::cerr << "Speedup file could not be opened.";
    exit(1);
  }
}

/* Linearly interpolates input speedup points. speedup_in is an array that
 * contains speedup values for 2, 4, 8, and 16 cores. speedup_out is an array
 * that has linearly interpolated values for 2-16 cores. The zeroth element of
 * speedup_out = 0, because there is no speedup with just one core.
 */
void BaseAllocator::InterpolateSpeedup(
    double* speedup_in, double* speedup_out) {
  // Copy the existing data points.
  for (int i = 0; i < NUM_SPEEDUP_POINTS; i++)
    speedup_out[cores[i]-1] = speedup_in[i];
  for (int i = 0; i < NUM_SPEEDUP_POINTS-1; i++) {
    double slope = (speedup_in[i+1]-speedup_in[i]) / (cores[i+1]-cores[i]);
    for (int j = cores[i]+1; j < cores[i+1]; j++) {  // Interpolate.
      speedup_out[j-1] = slope*(j - cores[i+1]) + speedup_in[i+1];
    }
  }
}

int BaseAllocator::get_cores_for_asid(int asid) {
  if (core_allocs->find(asid) != core_allocs->end())
    return core_allocs->operator[](asid);
  return 0;
}

}  // namespace xiosim
