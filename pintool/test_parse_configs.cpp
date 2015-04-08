/* Unit tests for parsing confuse-format Zesto configuration files.
 *
 * The test configuration file is found in config/default.cfg. It is tested with
 * only default values, and only certain values are tested; therefore, this unit
 * test is not by any means complete. It should only be used to generally verify
 * that nothing major broke.
 *
 * Author: Sam Xi
 */

#define CATCH_CONFIG_MAIN

#include <iostream>

#include "catch.hpp"
#include "zesto-structs.h"
#include "zesto-config.h"

#include "zesto-cache.h"  // Because zesto-exec doesn't include zesto-cache...
#include "zesto-commit.h"
#include "zesto-alloc.h"
#include "zesto-fetch.h"
#include "zesto-decode.h"
#include "zesto-dram.h"
#include "zesto-exec.h"
#include "zesto-noc.h"
#include "zesto-uncore.h"

// TODO: SUPER DUPER HACKEDY HACK.
// Because of really mangled dependencies in XIOSim (that I don't have time to
// address right now), this unit test depends on this method, which is defined
// in
// timing_sim. However, linking with timing_sim will produce a multiple main()
// function declaration error with this unit test. We need to do refactoring to
// fix this problem (along with many others), but as the unit test never
// actually needs this function to do anything, I'm just declaring an empty
// funcion to satisfy the linker.
void CheckIPCMessageQueue(bool isEarly, int caller_coreID) {}

TEST_CASE("Test configuration parsing", "config") {
    const char* config_file = "../config/default.cfg";
    const char* argv[2] = { "-config", config_file };
    read_config_file(2, argv, &knobs);

    // Only test a subset of the configuration parameters.
    SECTION("Checking system configuration") { CHECK(strcmp(knobs.model, "DPM") == 0); }

    SECTION("Checking general core configuration") {
        REQUIRE(knobs.default_cpu_speed == Approx(4000.0));
    }

    SECTION("Checking fetch stage configuration") {
        REQUIRE(knobs.fetch.IQ_size == 8);
        REQUIRE(knobs.fetch.byteQ_size == 4);

        SECTION("Checking branch prediction options") {
            REQUIRE(knobs.fetch.num_bpred_components == 1);
            REQUIRE(strcmp(knobs.fetch.bpred_opt_str[0], "2lev:gshare:1:1024:6:1") == 0);
        }

        SECTION("Checking L1 icache prefetcher options") {
            REQUIRE(knobs.memory.IL1_num_PF == 1);
            REQUIRE(strcmp(knobs.memory.IL1PF_opt_str[0], "nextline") == 0);
        }
    }

    SECTION("Checking decode stage configuration") {
        SECTION("Checking decoder max uops options") {
            REQUIRE(knobs.decode.num_decoder_specs == 4);
            REQUIRE(knobs.decode.decoders[0] == 4);
            REQUIRE(knobs.decode.decoders[1] == 1);
            REQUIRE(knobs.decode.decoders[2] == 1);
            REQUIRE(knobs.decode.decoders[3] == 1);
        }
    }

    SECTION("Checking alloc stage configuration") {
        REQUIRE(knobs.alloc.depth == 2);
        REQUIRE(knobs.alloc.width == 4);
        REQUIRE(knobs.alloc.drain_flush == false);
    }

    SECTION("Checking execution stage configuration") {
        SECTION("Checking L1 data cache prefetcher options") {
            REQUIRE(knobs.memory.DL1_num_PF == 1);
            REQUIRE(strcmp(knobs.memory.DL1PF_opt_str[0], "nextline") == 0);
        }

        SECTION("Checking L2 data cache prefetcher options") {
            REQUIRE(knobs.memory.DL2_num_PF == 1);
            REQUIRE(strcmp(knobs.memory.DL2PF_opt_str[0], "nextline") == 0);
        }

        SECTION("Checking integer ALU execution unit") {
            REQUIRE(knobs.exec.port_binding[FU_IEU].num_FUs == 2);
            REQUIRE(knobs.exec.fu_bindings[FU_IEU][0] == 0);
            REQUIRE(knobs.exec.fu_bindings[FU_IEU][1] == 1);
            REQUIRE(knobs.exec.latency[FU_IEU] == 1);
            REQUIRE(knobs.exec.issue_rate[FU_IEU] == 1);
        }
    }

    SECTION("Checking commit stage configuration") {
        REQUIRE(knobs.commit.ROB_size == 64);
        REQUIRE(knobs.commit.width == 4);
        REQUIRE(knobs.commit.branch_limit == 0);
        REQUIRE(knobs.commit.pre_commit_depth == -1);
    }

    SECTION("Checking uncore configuration") {
        REQUIRE(strcmp(LLC_opt_str, "LLC:2048:16:64:16:64:12:L:W:B:8:1:8:C") == 0);
        SECTION("Checking LLC prefecher options") {
            REQUIRE(LLC_num_PF == 1);
            REQUIRE(strcmp(LLC_PF_opt_str[0], "none") == 0);
        }
    }
}
