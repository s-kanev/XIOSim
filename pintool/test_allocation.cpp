/* Unit tests for the locally optimal and penalty policy allocators. These
 * require a non-trivial amount of initialization of global shared state in
 * order to run, even if they do not use them, because they are tightly
 * integrated into the rest of XIOSim.
 *
 * Author: Sam Xi
 */

#define CATCH_CONFIG_RUNNER

#include "assert.h"
#include "boost_interprocess.h"
#include "catch.hpp"  // Must come before interface.h
#include "../interface.h"
#include "../synchronization.h"
#include "multiprocess_shared.h"
#include "mpkeys.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <pthread.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>

#include "allocators_impl.h"
#include "parse_speedup.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"

struct locally_optimal_args {
    int asid;
    int loop_num;
    xiosim::BaseAllocator *allocator;
    int num_cores_alloc;
};
pthread_mutex_t cout_lock;

std::map<int, bool> ackTestMessages;
pthread_mutex_t ackTestMessages_lock;

const int NUM_CORES_TEST = 16;
const int NUM_TEST_PROCESSES = 2;
const size_t NUM_DATA_POINTS = 4;

TEST_CASE("Penalty allocator", "penalty") {
    using namespace xiosim;
    SHARED_VAR_INIT(int, num_processes);
    *num_processes = 3;
    char* filepath = "loop_speedup_data.csv";
    // We need a smaller allocation to actuall trigger the penalty policies.
    const int PENALTY_NUM_CORES = 8;
    PenaltyAllocator& core_allocator =
            reinterpret_cast<PenaltyAllocator&>(
                    AllocatorParser::Get(
                        "penalty", "energy", "logarithmic",
                        1, 8, PENALTY_NUM_CORES));
    LoadHelixSpeedupModelData(filepath);

    std::string loop_1("art_loop_2");
    std::string loop_2("art_loop_1");
    std::string loop_3("art_loop_1");
    int process_1 = 0;
    int process_2 = 1;
    int process_3 = 2;
    std::vector<double> scaling_1 = GetHelixLoopScaling(loop_1);
    double serial_runtime_1 = GetHelixFullLoopData(loop_1)->serial_runtime;
    std::vector<double> scaling_2 = GetHelixLoopScaling(loop_2);
    double serial_runtime_2 = GetHelixFullLoopData(loop_2)->serial_runtime;
    std::vector<double> scaling_3 = GetHelixLoopScaling(loop_3);
    double serial_runtime_3 = GetHelixFullLoopData(loop_3)->serial_runtime;

    REQUIRE(core_allocator.get_penalty_for_asid(process_1) == Approx(0));
    core_allocator.AllocateCoresForProcess(
            process_1, scaling_1, serial_runtime_1);
    REQUIRE(core_allocator.get_cores_for_asid(process_1) == 2);

    REQUIRE(core_allocator.get_penalty_for_asid(process_2) == Approx(0));
    core_allocator.AllocateCoresForProcess(
            process_2, scaling_2, serial_runtime_2);
    REQUIRE(core_allocator.get_cores_for_asid(process_2) == 4);

    REQUIRE(core_allocator.get_penalty_for_asid(process_3) == Approx(0));
    core_allocator.AllocateCoresForProcess(
            process_3, scaling_3, serial_runtime_3);
    REQUIRE(core_allocator.get_cores_for_asid(process_3) == 2);

    core_allocator.DeallocateCoresForProcess(process_1);
    REQUIRE(core_allocator.get_cores_for_asid(process_1) == 1);

    REQUIRE(core_allocator.get_penalty_for_asid(process_1) == Approx(0.682392));
    core_allocator.AllocateCoresForProcess(
            process_1, scaling_1, serial_runtime_1);
    REQUIRE(core_allocator.get_cores_for_asid(process_1) == 1);
    REQUIRE(core_allocator.get_penalty_for_asid(process_1) == Approx(-2.040744));
}

/* Thread function that calls the Allocate() method in the locally optimal
 * allocator. Tests whether the allocator properly waits for all threads to
 * check in before making a decision.
 */
void* TestLocallyOptimalPolicyThread(void* arg) {
    locally_optimal_args *args = (locally_optimal_args*) arg;
    xiosim::BaseAllocator *allocator = args->allocator;
    int asid = args->asid;
    std::stringstream loop_name;
    loop_name << "art_loop_" << args->loop_num;
    std::vector<double> loop_scaling = GetHelixLoopScaling(loop_name.str());
    double serial_runtime =
        GetHelixFullLoopData(loop_name.str())->serial_runtime;
    // Push a blocking ack message on to the queue.
    pthread_mutex_lock(&ackTestMessages_lock);
    ackTestMessages[asid] = false;
    pthread_mutex_unlock(&ackTestMessages_lock);
    // Initiate the core allocation request.
    args->num_cores_alloc = allocator->AllocateCoresForProcess(
            asid, loop_scaling,serial_runtime);
    pthread_mutex_lock(&ackTestMessages_lock);
    // If you are the last thread to check in, unblock all others.
    if (args->num_cores_alloc != -1) {
        for (auto it = ackTestMessages.begin(); it != ackTestMessages.end(); ++it) {
            if (it->second == false)
                ackTestMessages[it->first] = true;
        }
    } else {
        // Wait for the final process to check in.
        while (ackTestMessages[asid] == false) {
            pthread_mutex_unlock(&ackTestMessages_lock);
            usleep(1000);
            pthread_mutex_lock(&ackTestMessages_lock);
        }
        // Now get the actual allocation.
        args->num_cores_alloc= allocator->AllocateCoresForProcess(
                asid, loop_scaling,serial_runtime);
    }
    pthread_mutex_unlock(&ackTestMessages_lock);
#ifdef DEBUG
    pthread_mutex_lock(&cout_lock);
    std::cout << "Process " << asid << " was allocated " <<
            args->num_cores_alloc << " cores." << std::endl;
    pthread_mutex_unlock(&cout_lock);
#endif
    return NULL;
}

TEST_CASE("Locally optimal allocator", "local") {
    using namespace xiosim;
    SHARED_VAR_INIT(int, num_processes);
    *num_processes = NUM_TEST_PROCESSES;
    char* filepath = "loop_speedup_data.csv";
#ifdef DEBUG
    std::cout << "Number of processes: " << *num_processes << std::endl;
#endif
    const int NUM_TESTS = 4;
    BaseAllocator& core_allocator =
        AllocatorParser::Get("local", "throughput", "linear",
                             1, 8, NUM_CORES_TEST);
    LoadHelixSpeedupModelData(filepath);

    /* Initialize test data and correct output.
     * test_loops: Each column determines which loops to run.
     * Example: 2nd column = {1,2} means process 0 runs loop_1 and process 1
     * runs loop_2.
     */
    int test_loops[NUM_TEST_PROCESSES][NUM_TESTS] = {
        {1, 1, 2, 2},
        {1, 2, 1, 2}
    };
    /* correct_output: Each column is the correct number of cores to be
     * allocated to the process for that row.
     * Example: 2nd column
     */
    int correct_output[NUM_TEST_PROCESSES][NUM_TESTS] = {
        {8, 1, 15, 8},
        {8, 15, 1, 8}
    };

    // Initialize thread variables.
    pthread_mutex_init(&cout_lock, NULL);
    pthread_mutex_init(&ackTestMessages_lock, NULL);
    pthread_t threads[*num_processes];
    locally_optimal_args args[*num_processes];
    for (int i = 0; i < NUM_TESTS; i++) {
        for (int j = 0; j < *num_processes; j++) {
            args[j].allocator = &core_allocator;
            args[j].loop_num = test_loops[j][i];
            args[j].num_cores_alloc = 0;
            args[j].asid = j;
            pthread_create(&threads[j],
                           NULL,
                           TestLocallyOptimalPolicyThread,
                           (void*)&args[j]);
        }
        void* status;
        for (int j = 0; j < *num_processes; j++) {
            pthread_join(threads[j], &status);
        }
#ifdef DEBUG
        std::cout << "All threads have completed." << std::endl;
#endif
        for (int j = 0; j < *num_processes; j++) {
            REQUIRE(args[j].num_cores_alloc == correct_output[j][i]);
        }
    }
}

/* We need to call InitSharedState() to set up all the shared memory state
 * required by XIOSIM, but that requires some initial setup from the harness.
 * That setup is mimicked here.
 */
void SetupSharedState() {
    using namespace xiosim::shared;
    using namespace boost::interprocess;
    std::stringstream pidstream;
    pid_t test_pid = getpid();
    pidstream << test_pid;
    std::string shared_memory_key =
            pidstream.str() + std::string(XIOSIM_SHARED_MEMORY_KEY);
    std::string init_lock_key =
            pidstream.str() + std::string(XIOSIM_INIT_SHARED_LOCK);
    std::string counter_lock_key =
            pidstream.str() + std::string(XIOSIM_INIT_COUNTER_KEY);

    // To be safe, we use xiosim::shared::DEFAULT_SHARED_MEMORY_SIZE, or we
    // might run out of shared memory space in InitSharedState().
    global_shm = new managed_shared_memory(
            open_or_create,
            shared_memory_key.c_str(),
            DEFAULT_SHARED_MEMORY_SIZE);
    // InitSharedState() will expect this lock and counter to exist.
    named_mutex init_lock(open_or_create, init_lock_key.c_str());
    int *process_counter =
            global_shm->find_or_construct<int>(counter_lock_key.c_str())();
    *process_counter = 1;
    std::cout << "===========================" << std::endl
              << " Initializing shared state " << std::endl
              << "===========================" << std::endl;
    InitSharedState(false, test_pid, NUM_CORES_TEST);
}

int main(int argc, char* const argv[]) {
    // Create the shared memory cleanup struct.
    using namespace xiosim::shared;
    using namespace boost::interprocess;
    struct shm_remove {
        shm_remove() {
            shared_memory_object::remove(XIOSIM_SHARED_MEMORY_KEY);
        }
        ~shm_remove() {
            shared_memory_object::remove(XIOSIM_SHARED_MEMORY_KEY);
        }
    } remover;
    SetupSharedState();

    std::cout << "===========================" << std::endl
              << "    Beginning unit tests   " << std::endl
              << "===========================" << std::endl;
    int result = Catch::Session().run(argc, argv);
    std::cout << std::endl << "REMEMBER TO CLEAN UP /dev/shm!!" << std::endl;
    delete global_shm;
    return result;
}
