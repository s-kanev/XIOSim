/* Unit testing for the penalty policy allocator.
 *
 * Author: Sam Xi
 */

#include "boost_interprocess.h"
#include "assert.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <string>

#include "allocators_impl.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"

/* Assertions for testing and debugging. */
#define PRINT_ASSERTION_MESSAGE(val1, val2) \
  std::cerr << "Assertion failed: " << #val1 << " expected to be " << val2 << \
      ", was " << val1 << std::endl

#define ASSERT_EQUAL_INT(val1, val2) \
  do { \
    if (val1 != val2) { \
      PRINT_ASSERTION_MESSAGE(val1, val2); \
      std::exit(EXIT_FAILURE); \
    } \
  } while (false)

#define ASSERT_EQUAL_DOUBLE(val1, val2) \
  do { \
    if (!DoublesAreEqual(val1, val2)) { \
      PRINT_ASSERTION_MESSAGE(val1, val2); \
      std::exit(EXIT_FAILURE); \
    } \
  } while (false)

int num_cores = 16;
const double EPSILON = 0.00001;

/* A general way to compare two double values for equality up to EPSILON. */
bool DoublesAreEqual(double val1, double val2) {
  using namespace std;
  return (abs(val1 - val2) <= max(EPSILON, EPSILON*max(abs(val1), abs(val2))));
}

void TestPenaltyPolicy(xiosim::BaseAllocator *allocator) {
  // Correct output for this test.
  int num_tests = 5;
  int process_1_cores[5] = {11, 10, 10, 11, 10};
  double process_1_penalties[5] = {0, 0.4, -0.04375, -0.04375, 0.35625};
  int process_2_cores[5] = {5, 6, 4, 5, 6};
  double process_2_penalties[5] = {0, 0, 0.44375, -1.61625, -1.61625};

  xiosim::pid_cores_info data_p1;
  xiosim::pid_cores_info data_p2;
  std::string loop_1("loop_1");
  std::string loop_2("loop_2");
  int process_1 = 1;
  int process_2 = 2;
  int num_cores_1 = 0;
  int num_cores_2 = 0;

  for (int i = 0; i < num_tests; i++) {
    // Test for the current penalty.
    allocator->get_data_for_asid(process_1, &data_p1);
    ASSERT_EQUAL_DOUBLE(data_p1.current_penalty, process_1_penalties[i]);
    // Test for the correct core allocation.
    allocator->AllocateCoresForLoop(loop_1, process_1, &num_cores_1);
    ASSERT_EQUAL_INT(num_cores_1, process_1_cores[i]);

    // Test for the current penalty.
    allocator->get_data_for_asid(process_2, &data_p2);
    ASSERT_EQUAL_DOUBLE(data_p2.current_penalty, process_2_penalties[i]);
    // Test for the correct core allocation.
    allocator->AllocateCoresForLoop(loop_2, process_2, &num_cores_2);
    ASSERT_EQUAL_INT(num_cores_2, process_2_cores[i]);

    // Test that deallocation completes correctly.
    allocator->DeallocateCoresForProcess(process_1);
    allocator->get_data_for_asid(process_1, &data_p1);
    ASSERT_EQUAL_INT(data_p1.num_cores_allocated, 1);
    allocator->DeallocateCoresForProcess(process_1);
    allocator->get_data_for_asid(process_1, &data_p1);
    ASSERT_EQUAL_INT(data_p1.num_cores_allocated, 1);
  }
}

int main() {
  using namespace xiosim;
  char* filepath = "loop_speedup_data.csv";
  BaseAllocator *allocator = new PenaltyAllocator(num_cores);
  allocator->LoadHelixSpeedupModelData(filepath);
  TestPenaltyPolicy(allocator);

  std::cout << "Test completed successfully." << std::endl;

  return 0;
}
