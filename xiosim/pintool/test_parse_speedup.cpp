/* Unit tests for the speedup data parser. */

#include <string>
#include <vector>

#include "catch.hpp"
#include "parse_speedup.h"

TEST_CASE("Complete front-to-end test of the speedup data parser", "parser") {
    using namespace std;
    const char* filepath = "xiosim/pintool/test_data/loop_speedup_data.csv";
    LoadHelixSpeedupModelData(filepath);
    // Data has four points, and GetHelixLoopScaling() adds a fifth for 1 core.
    const int NUM_POINTS = 4;

    SECTION("test data for loop_1") {
        string loop_name = "loop_1";
        vector<double> loop_scaling = GetHelixLoopScaling(loop_name);
        REQUIRE(loop_scaling.size() == NUM_POINTS + 1);
        REQUIRE(loop_scaling.at(0) == Approx(1.0));
        REQUIRE(loop_scaling.at(1) == Approx(0.9));
        REQUIRE(loop_scaling.at(2) == Approx(1.6));
        REQUIRE(loop_scaling.at(3) == Approx(2.19));
        REQUIRE(loop_scaling.at(4) == Approx(2.39));
    }

    SECTION("test the weird loop name with weird speedup") {
        string loop_name = "a weird, weird loop name and speedup. $#$";
        vector<double> loop_scaling = GetHelixLoopScaling(loop_name);
        // The first three elements will be 0, 0.5, and 0.25 due to linear
        // interpolation but all the others should be 0.5.
        REQUIRE(loop_scaling.size() == NUM_POINTS + 1);
        REQUIRE(loop_scaling.at(0) == Approx(1));
        REQUIRE(loop_scaling.at(1) == Approx(1.5));
        REQUIRE(loop_scaling.at(2) == Approx(1.5));
        REQUIRE(loop_scaling.at(3) == Approx(1.5));
        REQUIRE(loop_scaling.at(4) == Approx(1.5));
    }

    SECTION("Test parsing of serial runtime and variance.") {
        string loop_name = "loop_1";
        loop_data* data = GetHelixFullLoopData(loop_name);
        REQUIRE(data->serial_runtime == 1111);
        REQUIRE(data->serial_runtime_variance == Approx(0.111));

        loop_name = "loop_2";
        data = GetHelixFullLoopData(loop_name);
        REQUIRE(data->serial_runtime == 22222);
        REQUIRE(data->serial_runtime_variance == Approx(0.5));
    }
}
