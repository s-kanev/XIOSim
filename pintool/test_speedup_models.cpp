/* Unit tests for the energy and throughput optimization functions under linear
 * and logarithmic speedup models.
 *
 * Author: Sam Xi
 */
#include <iostream>

#include "catch.hpp"
#include "speedup_models.h"

const int NUM_PROCESSES = 3;

TEST_CASE("Energy optimization under linear speedup") {
    std::map<int, int> core_allocs;
    std::vector<double> process_linear_scaling;
    std::vector<double> process_serial_runtime;
    int num_cores = 32;
    // The optimization for linear speedup doesn't depend on power values.
    LinearSpeedupModel speedup_model(.125, 1, num_cores, OptimizationTarget::ENERGY);

    // Set up fake test data.
    const int NUM_TESTS = 6;
    double test_serial_runtimes[NUM_TESTS][NUM_PROCESSES] = {
        { 18, 3, 1 }, { 27, 1, 1 }, { 25, 3, 1 }, { 9, 27, 1 }, { 2, 21, 1 }, { 24, 1, 1 }
    };
    double linear_scaling_factor = 1.1;  // Makes everything simpler.
    double correct_allocations[NUM_TESTS][NUM_PROCESSES] = {
        { 25, 5, 2 },  // This case MUST be tested with minimization.
        { 28, 2, 2 },
        { 26, 4, 2 },
        { 8, 23, 1 },
        { 3, 27, 2 },
        { 28, 2, 2 }
    };

    // Populate data structures.
    for (int i = 0; i < NUM_TESTS; i++) {
#ifdef DEBUG
        std::cout << "Test " << i << std::endl;
#endif
        for (int j = 0; j < 3; j++) {
            process_linear_scaling.push_back(linear_scaling_factor);
            process_serial_runtime.push_back(test_serial_runtimes[i][j]);
            core_allocs[j] = 1;
        }
        speedup_model.OptimizeForTarget(
            core_allocs, process_linear_scaling, process_serial_runtime);

        for (int j = 0; j < 3; j++) {
            REQUIRE(core_allocs[j] == correct_allocations[i][j]);
        }
        process_linear_scaling.clear();
        process_serial_runtime.clear();
        core_allocs.clear();
    }
}

TEST_CASE("Throughput optimization under linear scaling") {
    std::map<int, int> core_allocs;
    std::vector<double> process_scaling;
    std::vector<double> process_serial_runtime;
    int num_cores = 32;
    LinearSpeedupModel speedup_model(0.125, 1, num_cores, OptimizationTarget::THROUGHPUT);
    const int NUM_TESTS = 5;

    double test_serial_runtimes[NUM_TESTS][NUM_PROCESSES] = {
        { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }
    };

    double test_scaling_factors[NUM_TESTS][NUM_PROCESSES] = {
        { 1, 2, 3 }, { 1, 1, 1 }, { 3, 3, 1 }, { 2.54, 2.54, 1 }, { 1.13, 1.13, 1.13 }
    };

    double correct_allocations[NUM_TESTS][NUM_PROCESSES] = {
        { 1, 1, 30 }, { 10, 11, 11 }, { 15, 16, 1 }, { 15, 16, 1 }, { 10, 11, 11 }
    };

    // Populate data structures.
    for (int i = 0; i < NUM_TESTS; i++) {
#ifdef DEBUG
        std::cout << "Test " << i << std::endl;
#endif
        for (int j = 0; j < NUM_PROCESSES; j++) {
            process_scaling.push_back(test_scaling_factors[i][j]);
            process_serial_runtime.push_back(test_serial_runtimes[i][j]);
        }
        speedup_model.OptimizeForTarget(core_allocs, process_scaling, process_serial_runtime);

        for (int j = 0; j < NUM_PROCESSES; j++) {
            REQUIRE(core_allocs[j] == correct_allocations[i][j]);
        }
        process_scaling.clear();
        process_serial_runtime.clear();
        core_allocs.clear();
    }
}

TEST_CASE("LambertW function") {
    LogSpeedupModel speedup_model(0.125, 1, 32);
    REQUIRE(speedup_model.LambertW(0.05) == Approx(0.047672));
    REQUIRE(speedup_model.LambertW(0.1) == Approx(0.091277));
    REQUIRE(speedup_model.LambertW(0.3) == Approx(0.236755));
    REQUIRE(speedup_model.LambertW(1.0) == Approx(0.567143));
    REQUIRE(speedup_model.LambertW(1.5) == Approx(0.725861));
    REQUIRE(speedup_model.LambertW(2.0) == Approx(0.852605));
    REQUIRE(speedup_model.LambertW(2.5) == Approx(0.958586));
    REQUIRE(speedup_model.LambertW(3.0) == Approx(1.049908));
    REQUIRE(speedup_model.LambertW(3.5) == Approx(1.130289));
    REQUIRE(speedup_model.LambertW(4.0) == Approx(1.202158));
}

TEST_CASE("Energy optimization under logarithmic speedup") {
    std::map<int, int> core_allocs;
    std::vector<double> process_scaling;
    std::vector<double> process_serial_runtime;
    int num_cores = 32;
    LogSpeedupModel speedup_model(0.125, 1, num_cores, OptimizationTarget::ENERGY);

    const int NUM_TESTS = 18;
    double test_serial_runtimes[NUM_TESTS][NUM_PROCESSES] = { { 1, 1, 1 },
                                                              { 1, 1, 1 },
                                                              { 1, 1, 1 },
                                                              { 1, 1, 1 },
                                                              { 1, 1, 1 },
                                                              { 1, 1, 1 },

                                                              { 1, 2, 3 },
                                                              { 1, 2, 3 },
                                                              { 1, 2, 3 },
                                                              { 1, 2, 3 },
                                                              { 1, 2, 3 },
                                                              { 1, 2, 3 },

                                                              { 1, 5, 10 },
                                                              { 1, 5, 10 },
                                                              { 1, 5, 10 },
                                                              { 1, 5, 10 },
                                                              { 1, 5, 10 },
                                                              { 1, 5, 10 } };

    double test_scaling_factors[NUM_TESTS][NUM_PROCESSES] = { { 2.5, 1.818, 1.818 },
                                                              { 5, 2.5, 10 },
                                                              { 1, 2.5, 5 },
                                                              { 20, 2.857, 3.333 },
                                                              { 2.222, 10, 4 },
                                                              { 20, 20, 20 },

                                                              { 5, 20, 20 },
                                                              { 5, 4, 20 },
                                                              { 1, 1, 20 },
                                                              { 20, 2.857, 20 },
                                                              { 1, 1, 5 },
                                                              { 10, 10, 10 },

                                                              { 2.857, 1.0526, 20 },
                                                              { 1.111, 5, 20 },
                                                              { 20, 2.857, 20 },
                                                              { 5, 5, 20 },
                                                              { 1, 1.0526, 1.176 },
                                                              { 10, 1.818, 1.818 } };

    double correct_allocations[NUM_TESTS][NUM_PROCESSES] = { { 3, 4, 4 },
                                                             { 3, 6, 3 },
                                                             { 5, 2, 2 },
                                                             { 3, 5, 4 },
                                                             { 6, 3, 3 },
                                                             { 5, 5, 5 },

                                                             { 7, 3, 5 },
                                                             { 2, 7, 3 },
                                                             { 2, 5, 3 },
                                                             { 3, 7, 3 },
                                                             { 2, 5, 2 },
                                                             { 3, 4, 7 },

                                                             { 2, 5, 3 },
                                                             { 3, 7, 3 },
                                                             { 3, 7, 3 },
                                                             { 2, 7, 3 },
                                                             { 1, 2, 5 },
                                                             { 3, 2, 6 } };

    for (int i = 0; i < NUM_TESTS; i++) {
#ifdef DEBUG
        std::cout << "Test " << i << std::endl;
#endif
        for (int j = 0; j < NUM_PROCESSES; j++) {
            process_scaling.push_back(test_scaling_factors[i][j]);
            process_serial_runtime.push_back(test_serial_runtimes[i][j]);
            core_allocs[j] = 1;
        }
        speedup_model.OptimizeForTarget(core_allocs, process_scaling, process_serial_runtime);

        for (int j = 0; j < NUM_PROCESSES; j++) {
            REQUIRE(core_allocs[j] == correct_allocations[i][j]);
        }
        process_scaling.clear();
        process_serial_runtime.clear();
        core_allocs.clear();
    }
}

TEST_CASE("Throughput optimization under logarithmic scaling") {
    std::map<int, int> core_allocs;
    std::vector<double> process_scaling;
    std::vector<double> process_serial_runtime;
    int num_cores = 32;
    LogSpeedupModel speedup_model(0.125, 1, num_cores, OptimizationTarget::THROUGHPUT);
    const int NUM_TESTS = 8;

    // These don't matter for throughput optimization.
    double test_serial_runtimes[NUM_TESTS][NUM_PROCESSES] = { { 1, 1, 1 },
                                                              { 1, 1, 1 },
                                                              { 1, 1, 1 },
                                                              { 1, 1, 1 },
                                                              { 1, 1, 1 },
                                                              { 1, 1, 1 },
                                                              { 1, 1, 1 },
                                                              { 1, 1, 1 } };

    double test_scaling_factors[NUM_TESTS][NUM_PROCESSES] = { { 1, 2, 3 },
                                                              { 1, 1, 1 },
                                                              { 3, 3, 1 },
                                                              { 2.54, 2.54, 1 },
                                                              { 1.13, 1.13, 1.13 },
                                                              { 1, 5, 10.1 },
                                                              { 5, 9, 13.5 },
                                                              { 1, 20, 20 } };

    double correct_allocations[NUM_TESTS][NUM_PROCESSES] = { { 5, 11, 16 },
                                                             { 11, 11, 10 },
                                                             { 14, 13, 5 },
                                                             { 14, 13, 5 },
                                                             { 11, 11, 10 },
                                                             { 2, 10, 20 },
                                                             { 6, 10, 16 },
                                                             { 1, 16, 15 } };

    // Populate data structures.
    for (int i = 0; i < NUM_TESTS; i++) {
#ifdef DEBUG
        std::cout << "Test " << i << std::endl;
#endif
        for (int j = 0; j < NUM_PROCESSES; j++) {
            process_scaling.push_back(test_scaling_factors[i][j]);
            process_serial_runtime.push_back(test_serial_runtimes[i][j]);
        }
        speedup_model.OptimizeForTarget(core_allocs, process_scaling, process_serial_runtime);
        /* If multiple processes have the same scaling factors, then any
         * permutation of their allocations is valid. */
        for (int j = 0; j < NUM_PROCESSES; j++) {
            bool is_similar = false;
            bool test = false;
            for (int k = j + 1; k < NUM_PROCESSES; k++) {
                if (test_scaling_factors[i][j] == Approx(test_scaling_factors[i][k])) {
                    is_similar = true;
                    // Catch doesn't support any boolean expressions with more
                    // than a single comparison, so we have to test this
                    // manually and fail with a descriptive message.
                    test = test || core_allocs[j] == correct_allocations[i][j] ||
                           core_allocs[j] == correct_allocations[i][k];
                    if (test) {
                        // If the current processes's core allocation checks
                        // out, then we don't need to check the other scaling
                        // factors.
                        REQUIRE(test);
                        break;
                    }
                } else {
                    REQUIRE(core_allocs[j] == correct_allocations[i][j]);
                    break;
                }
            }
            if (is_similar && !test) {
                // If we get here, then we didn't match any of the valid
                // allocation solutions.
                FAIL("Core allocation of " << core_allocs[j] << " for process " << j
                                           << " with similar scaling did not match any valid "
                                              "permutations.");
            }
        }

        process_scaling.clear();
        process_serial_runtime.clear();
        core_allocs.clear();
    }
}
