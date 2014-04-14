#define CATCH_CONFIG_MAIN
#include <string>
#include <vector>

#include "catch.hpp"
#include "parse_speedup.h"

TEST_CASE("Complete front-to-end test of the speedup data parser", "parser") {
    using namespace std;
    const char* filepath = "loop_speedup_data.csv";
    LoadHelixSpeedupModelData(filepath);
    const int NUM_CORES = 16;

    SECTION("test data for loop_1") {
        string loop_name = "loop_1";
        vector<double> loop_scaling = GetHelixLoopScaling(loop_name);
        REQUIRE(loop_scaling.size() == NUM_CORES);
        REQUIRE(loop_scaling.at(0) == Approx(0.0));
        REQUIRE(loop_scaling.at(1) == Approx(0.9));
        REQUIRE(loop_scaling.at(2) == Approx(0.8));
        REQUIRE(loop_scaling.at(3) == Approx(0.7));
        REQUIRE(loop_scaling.at(4) == Approx(0.6725));
        REQUIRE(loop_scaling.at(5) == Approx(0.645));
        REQUIRE(loop_scaling.at(6) == Approx(0.6175));
        REQUIRE(loop_scaling.at(7) == Approx(0.59));
        REQUIRE(loop_scaling.at(8) == Approx(0.54125));
        REQUIRE(loop_scaling.at(9) == Approx(0.4925));
        REQUIRE(loop_scaling.at(10) == Approx(0.44375));
        REQUIRE(loop_scaling.at(11) == Approx(0.395));
        REQUIRE(loop_scaling.at(12) == Approx(0.34625));
        REQUIRE(loop_scaling.at(13) == Approx(0.2975));
        REQUIRE(loop_scaling.at(14) == Approx(0.24875));
        REQUIRE(loop_scaling.at(15) == Approx(0.2));
    }

    SECTION("test the weird loop name with weird speedup") {
        string loop_name = "a weird, weird loop name and speedup. $#$";
        vector<double> loop_scaling = GetHelixLoopScaling(loop_name);
        // The first three elements will be 0, 0.5, and 0.25 due to linear
        // interpolation but all the others should be 0.5.
        REQUIRE(loop_scaling.at(0) == Approx(0.0));
        REQUIRE(loop_scaling.at(1) == Approx(0.5));
        REQUIRE(loop_scaling.at(2) == Approx(0.25));
        for (int i = 3; i < NUM_CORES; i++) {
          REQUIRE(loop_scaling.at(i) == Approx(0.0));
        }
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
