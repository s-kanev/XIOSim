/* Unit testing for the penalty policy allocator.
 *
 * Author: Sam Xi
 */

#include "boost_interprocess.h"
#include "../interface.h"
#include "../synchronization.h"
#include "multiprocess_shared.h"
#include "assert.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <pthread.h>
#include <sstream>
#include <string>
#include <sys/types.h>

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

struct locally_optimal_args {
  int asid;
  int loop_num;
  xiosim::BaseAllocator *allocator;
  int num_cores_alloc;
};
pthread_mutex_t cout_lock;

// boost::interprocess::managed_shared_memory *global_shm;
const char* SHARED_MEMORY_NAME = "shared_memory_unit_test";
const std::size_t DEFAULT_SIZE = 1024;

const int num_cores = 16;
const int NUM_TEST_PROCESSES = 2;
const int NUM_TESTS = 6;
const double EPSILON = 0.00001;

/* A general way to compare two double values for equality up to EPSILON. */
bool DoublesAreEqual(double val1, double val2) {
  using namespace std;
  return (abs(val1 - val2) <= max(EPSILON, EPSILON*max(abs(val1), abs(val2))));
}

void TestPenaltyPolicy() {
  using namespace xiosim;
  char* filepath = "loop_speedup_data.csv";
  BaseAllocator *core_allocator = new PenaltyAllocator(num_cores);
  core_allocator->LoadHelixSpeedupModelData(filepath);

  // Correct output for this test.
  int num_tests = 5;
  int process_1_cores[5] = {11, 10, 11, 10, 11};
  double process_1_penalties[5] = {-1, 0.4, -0.04375, 0.35625, -0.0875};
  int process_2_cores[5] = {5, 6, 5, 6, 5};
  double process_2_penalties[5] = {-1, 0, 0, 0, 0};

  std::string loop_1("loop_1");
  std::string loop_2("loop_2");
  int process_1 = 1;
  int process_2 = 2;
  int num_cores_1 = 0;
  int num_cores_2 = 0;

  for (int i = 0; i < num_tests; i++) {
    // Test for the current penalty.
    ASSERT_EQUAL_DOUBLE(
        ((PenaltyAllocator*)core_allocator)->get_penalty_for_asid(process_1),
        process_1_penalties[i]);
    core_allocator->AllocateCoresForLoop(loop_1, process_1, &num_cores_1);
    // Test for the correct core allocation.
    ASSERT_EQUAL_INT(core_allocator->get_cores_for_asid(
        process_1), process_1_cores[i]);

    // Test for the current penalty.
    ASSERT_EQUAL_DOUBLE(
        ((PenaltyAllocator*)core_allocator)->get_penalty_for_asid(process_2),
        process_2_penalties[i]);
    core_allocator->AllocateCoresForLoop(loop_2, process_2, &num_cores_2);
    // Test for the correct core allocation.
    ASSERT_EQUAL_INT(core_allocator->get_cores_for_asid(
        process_2), process_2_cores[i]);

    // Test that deallocation completes correctly.
    core_allocator->DeallocateCoresForProcess(process_1);
    ASSERT_EQUAL_INT(core_allocator->get_cores_for_asid(process_1), 1);
    core_allocator->DeallocateCoresForProcess(process_2);
    ASSERT_EQUAL_INT(core_allocator->get_cores_for_asid(process_2), 1);
  }
  delete core_allocator;
}

void* TestLocallyOptimalPolicyThread(void* arg) {
  locally_optimal_args *args = (locally_optimal_args*) arg;
  xiosim::BaseAllocator *allocator = args->allocator;
  int asid = args->asid;
  std::stringstream loop_name;
  loop_name << "loop_" << args->loop_num;
  int status = allocator->AllocateCoresForLoop(
      loop_name.str(), asid, &(args->num_cores_alloc));
#ifdef DEBUG
  pthread_mutex_lock(&cout_lock);
  std::cout << "Allocation return code: " << status << std::endl;
  std::cout << "Process " << asid << " was allocated " <<
      args->num_cores_alloc << " cores." << std::endl;
  pthread_mutex_unlock(&cout_lock);
#endif
  return NULL;
}

void TestLocallyOptimalPolicy() {
  // Initialize shared memory segments and variables BEFORE creating the
  // allocator!
  using namespace xiosim;
  using namespace boost::interprocess;
  struct shm_remove {
    shm_remove() { shared_memory_object::remove(SHARED_MEMORY_NAME); }
    ~shm_remove() { shared_memory_object::remove(SHARED_MEMORY_NAME); }
  } remover;
  global_shm = new managed_shared_memory(
      open_or_create, SHARED_MEMORY_NAME, DEFAULT_SIZE);
  SHARED_VAR_INIT(int, num_processes);
  *num_processes = NUM_TEST_PROCESSES;
  char* filepath = "loop_speedup_data.csv";
#ifdef DEBUG
  std::cout << "Number of processes: " << *num_processes << std::endl;
#endif
  BaseAllocator *core_allocator = new LocallyOptimalAllocator(num_cores);
  core_allocator->LoadHelixSpeedupModelData(filepath);

  /* Initialize test data and correct output.
   * test_loops: Each column determines which loops to run.
   * Example: 2nd column = {1,2} means process 0 runs loop_1 and process 1 runs
   * loop_2.
   */
  int test_loops[NUM_TEST_PROCESSES][NUM_TESTS] = {
    {1, 1, 2, 2, 3, 3},
    {1, 2, 3, 1, 1, 3}
  };
  /* correct_output: Each column is the correct number of cores to be allocated
   * to the process for that row.
   * Example: 2nd column
   */
  int correct_output[NUM_TEST_PROCESSES][NUM_TESTS] = {
    {8, 11, 8, 5, 6, 8},
    {8, 5, 8, 11, 10, 8}
  };

  // Initialize thread variables.
  pthread_mutex_init(&cout_lock, NULL);
  pthread_t threads[*num_processes];
  locally_optimal_args args[*num_processes];
  for (int i = 0; i < NUM_TESTS; i++) {
    for (int j = 0; j < *num_processes; j++) {
      args[j].allocator = core_allocator;
      args[j].loop_num = test_loops[j][i];
      args[j].num_cores_alloc = 0;
      args[j].asid = j;
      pthread_create(
          &threads[j], NULL, TestLocallyOptimalPolicyThread, (void*)&args[j]);
    }
    void* status;
    for (int j = 0; j < *num_processes; j++) {
      pthread_join(threads[j], &status);
    }
#ifdef DEBUG
    std::cout << "All threads have completed." << std::endl;
#endif
    for (int j = 0; j < *num_processes; j++) {
      ASSERT_EQUAL_INT(args[j].num_cores_alloc, correct_output[j][i]);
    }
  }
  delete core_allocator;
  delete global_shm;
}


int main() {
  TestPenaltyPolicy();
  TestLocallyOptimalPolicy();

  std::cout << "Test completed successfully." << std::endl;

  return 0;
}
