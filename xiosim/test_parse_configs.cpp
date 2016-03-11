/* Unit tests for parsing confuse-format Zesto configuration files.
 *
 * The test configuration file is found in config/default.cfg. It is tested with
 * only default values, and only certain values are tested; therefore, this unit
 * test is not by any means complete. It should only be used to generally verify
 * that nothing major broke.
 *
 * Author: Sam Xi
 */

#include <cstring>
#include <iostream>

#include "catch.hpp"
#include "zesto-config.h"

const std::string XIOSIM_PACKAGE_PATH = "xiosim/";

TEST_CASE("Test configuration parsing", "config") {
    using namespace xiosim;

    core_knobs_t core_knobs;
    uncore_knobs_t uncore_knobs;
    system_knobs_t system_knobs;

    std::string config_file = XIOSIM_PACKAGE_PATH + "config/default.cfg";
    read_config_file(config_file, &core_knobs, &uncore_knobs, &system_knobs);

    // Only test a subset of the configuration parameters.
    SECTION("Checking system configuration") { CHECK(strcmp(core_knobs.model, "DPM") == 0); }

    SECTION("Checking general core configuration") {
        REQUIRE(core_knobs.default_cpu_speed == Approx(4000.0));
    }

    SECTION("Checking fetch stage configuration") {
        REQUIRE(core_knobs.fetch.IQ_size == 8);
        REQUIRE(core_knobs.fetch.byteQ_size == 4);

        SECTION("Checking branch prediction options") {
            REQUIRE(core_knobs.fetch.num_bpred_components == 1);
            REQUIRE(strcmp(core_knobs.fetch.bpred_opt_str[0], "2lev:gshare:1:1024:6:1") == 0);
        }

        SECTION("Checking L1 icache prefetcher options") {
            REQUIRE(core_knobs.memory.IL1_pf.num_pf == 1);
            REQUIRE(strcmp(core_knobs.memory.IL1_pf.pf_opt_str[0], "nextline") == 0);
        }
    }

    SECTION("Checking decode stage configuration") {
        SECTION("Checking decoder max uops options") {
            REQUIRE(core_knobs.decode.max_uops.size() == 4);
            REQUIRE(core_knobs.decode.max_uops[0] == 4);
            REQUIRE(core_knobs.decode.max_uops[1] == 1);
            REQUIRE(core_knobs.decode.max_uops[2] == 1);
            REQUIRE(core_knobs.decode.max_uops[3] == 1);
        }
    }

    SECTION("Checking alloc stage configuration") {
        REQUIRE(core_knobs.alloc.depth == 2);
        REQUIRE(core_knobs.alloc.width == 4);
        REQUIRE(core_knobs.alloc.drain_flush == false);
    }

    SECTION("Checking execution stage configuration") {
        SECTION("Checking L1 data cache prefetcher options") {
            REQUIRE(core_knobs.memory.DL1_pf.num_pf == 1);
            REQUIRE(strcmp(core_knobs.memory.DL1_pf.pf_opt_str[0], "nextline") == 0);
        }

        SECTION("Checking L2 data cache prefetcher options") {
            REQUIRE(core_knobs.memory.DL2_pf.num_pf == 1);
            REQUIRE(strcmp(core_knobs.memory.DL2_pf.pf_opt_str[0], "nextline") == 0);
        }

        SECTION("Checking integer ALU execution unit") {
            REQUIRE(core_knobs.exec.port_binding[FU_IEU].num_FUs == 2);
            REQUIRE(core_knobs.exec.port_binding[FU_IEU].ports[0] == 0);
            REQUIRE(core_knobs.exec.port_binding[FU_IEU].ports[1] == 1);
            REQUIRE(core_knobs.exec.latency[FU_IEU] == 1);
            REQUIRE(core_knobs.exec.issue_rate[FU_IEU] == 1);
        }
    }

    SECTION("Checking commit stage configuration") {
        REQUIRE(core_knobs.commit.ROB_size == 64);
        REQUIRE(core_knobs.commit.width == 4);
        REQUIRE(core_knobs.commit.branch_limit == 0);
        REQUIRE(core_knobs.commit.pre_commit_depth == -1);
    }

    SECTION("Checking uncore configuration") {
        REQUIRE(strcmp(uncore_knobs.LLC_opt_str, "LLC:2048:16:64:16:64:12:L:W:B:8:1:8:C") == 0);
        SECTION("Checking LLC prefecher options") {
            REQUIRE(uncore_knobs.LLC_pf.num_pf == 1);
            REQUIRE(strcmp(uncore_knobs.LLC_pf.pf_opt_str[0], "none") == 0);
        }
    }

    free_config();
}
