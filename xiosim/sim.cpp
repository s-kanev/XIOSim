/*
 * sim.cpp -- global libsim state. Init / deinit.
 * Copyright, Svilen Kanev, 2011
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/io.h>

#include <map>
#include <cstddef>

#include "confuse.h"
#include "host.h"
#include "misc.h"
#include "memory.h"
#include "stats.h"
#include "sim.h"
#include "slices.h"
#include "synchronization.h"
#include "decode.h"

#include "zesto-config.h"
#include "zesto-core.h"
#include "zesto-fetch.h"
#include "zesto-oracle.h"
#include "zesto-decode.h"
#include "zesto-bpred.h"
#include "zesto-alloc.h"
#include "zesto-exec.h"
#include "zesto-commit.h"
#include "zesto-dram.h"
#include "zesto-uncore.h"
#include "zesto-MC.h"
#include "zesto-power.h"
#include "zesto-dvfs.h"
#include "zesto-repeater.h"

#include "sim-loop.h"
#include "libsim.h"

/* libconfuse options database */
extern cfg_t* all_opts;

/* stats database */
xiosim::stats::StatsDatabase* sim_sdb;

/* power stats database */
xiosim::stats::StatsDatabase* rtp_sdb;

/* microarchitecture state */
struct core_t** cores = NULL;

/* microarchitecture configuration parameters/knobs */
struct core_knobs_t knobs;

/* number of cores */
int num_cores = 1;

/* redirected program/simulator output file names */
const char* sim_simout = nullptr;

/* random number generator seed */
int rand_seed;

/* execution start/end times */
int sim_elapsed_time;

/* spin on assertion failure so we can attach a debbuger */
bool assert_spin;

/* extern so we don't bring in all of multiprocess_shared.h */
extern int* num_processes;

namespace xiosim {
namespace libsim {

static void create_modules(void);
static void sim_print_stats(FILE* fd);
void on_assert_fail(int coreID);

int init(int argc, char** argv) {
    char* s;
    int i;

    /* Parse Zesto configuration file. This will populate both knobs and all_opts. */
    // TODO: Get rid of c-style cast.
    read_config_file(argc, (const char**)argv, &knobs);

    /* redirect I/O? */
    if (sim_simout != NULL) {
        fflush(stderr);
        if (!freopen(sim_simout, "w", stderr)) {
            /* Print error message to stdout too, stderr might be messed up. */
            fprintf(stdout, "unable to redirect simulator output to file `%s'\n", sim_simout);
            fflush(stdout);
            fatal("unable to redirect simulator output to file `%s'", sim_simout);
        }
    }

    /* seed the random number generator */
    if (rand_seed == 0) {
        /* seed with the timer value, true random */
        srand(time((time_t*)NULL));
    } else {
        /* seed with default or user-specified random number generator seed */
        srand(rand_seed);
    }

    /* initialize the instruction decoder */
    xiosim::x86::init_decoder();

    /* Initialize tracing */
    ztrace_init();

    register_assert_fail_handler(on_assert_fail);

    /* Initialize virtual memory */
    xiosim::memory::init(*num_processes);

    /* initialize all simulation modules */
    create_modules();

    /* initialize simulator loop state */
    sim_loop_init();

    /* register all simulator stats */
    sim_sdb = stat_new();
    sim_reg_stats(sim_sdb);

    /* stat database for power computation */
    rtp_sdb = stat_new();
    sim_reg_stats(rtp_sdb);
    stat_save_stats(rtp_sdb);

    /* record start of execution time, used in rate stats */
    time_t sim_start_time = time((time_t*)NULL);

    /* emit the command line for later reuse */
    fprintf(stderr, "sim: command line: ");
    for (i = 0; i < argc; i++)
        fprintf(stderr, "%s ", argv[i]);
    fprintf(stderr, "\n");

    /* output simulation conditions */
    s = ctime(&sim_start_time);
    if (s[strlen(s) - 1] == '\n')
        s[strlen(s) - 1] = '\0';
    fprintf(stderr, "\nsim: simulation started @ %s, options follow:\n", s);
    char buff[128];
    gethostname(buff, sizeof(buff));
    fprintf(stderr, "Executing on host: %s\n", buff);

    cfg_print(all_opts, stderr);
    fprintf(stderr, "\n");

    if (knobs.power.compute)
        init_power();

    return 0;
}

void deinit() {
    /* scale stats if running multiple simulation slices */
    scale_all_slices();

    /* print simulator stats */
    sim_print_stats(stderr);
    if (knobs.power.compute) {
        stat_save_stats(sim_sdb);
        compute_power(sim_sdb, true);
        deinit_power();
    }

    repeater_shutdown(knobs.exec.repeater_opt_str);

    /* If captured, print out ztrace */
    for (int i = 0; i < num_cores; i++)
        cores[i]->oracle->trace_in_flight_ops();
    ztrace_flush();

    /* Print instruction type histograms */
    for (int i = 0; i < num_cores; i++)
        cores[i]->oracle->dump_instruction_histograms("iclass_hist", "iform_hist");

    for (int i = 0; i < num_cores; i++)
        if (cores[i]->stat.oracle_unknown_insn / (double)cores[i]->stat.oracle_total_insn > 0.02)
            fprintf(stderr,
                    "WARNING: [%d] More than 2%% instructions turned to NOPs (%" PRId64
                    " out of %" PRId64 ")\n",
                    i,
                    cores[i]->stat.oracle_unknown_insn,
                    cores[i]->stat.oracle_total_insn);

    // Free memory allocated by libconfuse for the configuration options.
    cfg_free(all_opts);
}

/* initialize core state, etc. - called AFTER config parameters have been parsed */
static void create_modules(void) {
    uncore_create();
    dram_create();

    /* initialize microarchitecture state */
    cores = (struct core_t**)calloc(num_cores, sizeof(*cores));
    if (!cores)
        fatal("failed to calloc cores");
    for (int i = 0; i < num_cores; i++) {
        cores[i] = new core_t(i);
        if (!cores[i])
            fatal("failed to calloc cores[]");

        cores[i]->knobs = &knobs;
    }

    // Needs to be called before creating core->exec
    repeater_init(knobs.exec.repeater_opt_str);

    for (int i = 0; i < num_cores; i++) {
        cores[i]->oracle = new core_oracle_t(cores[i]);
        cores[i]->commit = commit_create(knobs.model, cores[i]);
        cores[i]->exec = exec_create(knobs.model, cores[i]);
        cores[i]->alloc = alloc_create(knobs.model, cores[i]);
        cores[i]->decode = decode_create(knobs.model, cores[i]);
        cores[i]->fetch = fetch_create(knobs.model, cores[i]);
        cores[i]->power = power_create(knobs.model, cores[i]);
        cores[i]->vf_controller = vf_controller_create(knobs.dvfs_opt_str, cores[i]);
    }

    if (strcasecmp(knobs.dvfs_opt_str, "none") && strcasecmp(knobs.exec.repeater_opt_str, "none"))
        fatal("DVFS is not compatible with the memory repeater.");
}

/* register simulation statistics */
void sim_reg_stats(xiosim::stats::StatsDatabase* sdb) {
    using namespace xiosim::stats;
    int i;
    bool is_DPM = strcasecmp(knobs.model, "STM") != 0;

    /* These stats must come first. */
    auto& sim_cycle_st = stat_reg_counter(
            sdb, true, "sim_cycle", "total simulation cycles (CPU cycles assuming default freq)",
            &uncore->default_cpu_cycles, 0, TRUE, NULL);
    stat_reg_double(sdb, true, "sim_time", "total simulated time (us)", &uncore->sim_time, 0.0,
                    TRUE, NULL);
    auto& sim_elapsed_time_st = stat_reg_int(sdb, true, "sim_elapsed_time",
                                             "total simulation time in seconds",
                                             &sim_elapsed_time, 0, true, NULL);
    stat_reg_formula(sdb, true, "sim_cycle_rate",
                     "simulation speed (in Mcycles/sec)",
                     sim_cycle_st / (sim_elapsed_time_st * Constant(1000000.0)), NULL);

    /* per core stats */
    for (i = 0; i < num_cores; i++)
        cores[i]->reg_stats(sdb);

    uncore_reg_stats(sdb);
    xiosim::memory::reg_stats(sdb);

    stat_reg_note(sdb, "\n#### SIMULATOR PERFORMANCE STATS ####");

    Formula all_insn("all_insn", "total insts simulated for all cores", "%12.0f");
    Formula sim_inst_rate("sim_inst_rate", "simulation speed (in MIPS)");
    Formula all_uops("all_uops", "total uops simulated for all cores", "%12.0f");
    Formula sim_uop_rate("sim_uop_rate", "simulation speed (in MuPS)");

    /* Incrementally build each formula.  We can't yet define a formula in terms
     * of another, so each one needs to be constructed separately.
     */
    for (i = 0; i < num_cores; i++) {
        auto commit_insn_st = stat_find_core_stat<counter_t>(sdb, i, "commit_insn");
        assert(commit_insn_st);
        all_insn += *commit_insn_st;
        sim_inst_rate += *commit_insn_st;

        auto commit_uops_st = stat_find_core_stat<counter_t>(sdb, i, "commit_uops");
        assert(commit_uops_st);
        all_uops += *commit_uops_st;
        sim_uop_rate += *commit_uops_st;
    }
    sim_inst_rate /= (sim_elapsed_time_st * Constant(1000000.0));
    sim_uop_rate /= (sim_elapsed_time_st * Constant(1000000.0));
    stat_reg_formula(sdb, all_insn);
    stat_reg_formula(sdb, all_uops);
    stat_reg_formula(sdb, sim_inst_rate);
    stat_reg_formula(sdb, sim_uop_rate);

    if (is_DPM) {
        Formula all_eff_uops("all_eff_uops", "total effective uops simulated for all cores",
                             "%12.0f");
        Formula sim_eff_uop_rate("sim_eff_uop_rate", "simulation speed (in MeuPS)");
        for (i = 0; i < num_cores; i++) {
            auto commit_eff_uops_st = stat_find_core_stat<counter_t>(sdb, i, "commit_eff_uops");
            assert(commit_eff_uops_st);
            all_eff_uops += *commit_eff_uops_st;
            sim_eff_uop_rate += *commit_eff_uops_st;
        }
        sim_eff_uop_rate /= (sim_elapsed_time_st * Constant(1000000.0));
        stat_reg_formula(sdb, sim_eff_uop_rate);
    }

    // Total IPC formulas.
    if (num_cores == 1) {
        auto c0_commit_insn_st = stat_find_stat<counter_t>(sdb, "c0.commit_insn");
        auto c0_sim_cycle_st = stat_find_stat<tick_t>(sdb, "c0.sim_cycle");
        stat_reg_formula(sdb, true, "total_IPC", "final commit IPC",
                         *c0_commit_insn_st / *c0_sim_cycle_st, NULL);
    } else {
        // Compute geometric means of IPC.
        Formula gm_ipc("GM_IPC", "geometric mean IPC across all cores");
        Formula gm_upc("GM_uPC", "geometric mean uPC across all cores");
        for (i = 0; i < num_cores; i++) {
            auto core_commit_ipc_st = stat_find_core_stat<counter_t>(sdb, i, "commit_IPC");
            auto core_commit_upc_st = stat_find_core_stat<counter_t>(sdb, i, "commit_uPC");
            assert(core_commit_ipc_st && core_commit_upc_st);
            gm_ipc += *core_commit_ipc_st;
            gm_upc += *core_commit_upc_st;
        }
        gm_ipc ^= Constant(1.0 / num_cores);
        gm_upc ^= Constant(1.0 / num_cores);
        stat_reg_formula(sdb, gm_ipc);
        stat_reg_formula(sdb, gm_upc);

        if (is_DPM) {
            Formula gm_eff_upc("GM_euPC", "geometric mean euPC across all cores");
            for (i = 0; i < num_cores; i++) {
                auto core_commit_eupc_st = stat_find_core_stat<counter_t>(sdb, i, "commit_euPC");
                assert(core_commit_eupc_st);
                gm_upc += *core_commit_eupc_st;
            }
            gm_eff_upc ^= Constant(1.0 / num_cores);
            stat_reg_formula(sdb, gm_eff_upc);
        }
    }
}

void sim_print_stats(FILE* fd) {
    /* print simulation stats */
    fprintf(fd, "\nsim: ** simulation statistics **\n");
    stat_print_stats(sim_sdb, fd);
    fprintf(fd, "\n");
    fflush(fd);
}

void compute_rtp_power(void) {
    stat_save_stats_delta(rtp_sdb);  // Store delta values for translation
    compute_power(rtp_sdb, false);
    stat_save_stats(rtp_sdb);  // Create new checkpoint for next delta
}

/* On assertion failure, dump ztrace. Potentially spin so we can attach a debugger. */
void on_assert_fail(int coreID) {
    if (coreID != xiosim::INVALID_CORE) {
        core_t* core = cores[coreID];
        fprintf(stderr, "core: %d, cycle: %" PRId64", num_Mops: %" PRId64"\n", coreID, core->sim_cycle, core->stat.oracle_total_insn);
        fprintf(stderr, "PC: %" PRIxPTR", pin->PC: %" PRIxPTR", pin->NPC: %" PRIxPTR"\n", core->fetch->PC, core->fetch->feeder_PC, core->fetch->feeder_NPC);
    }
    fflush(stderr);

    for (int i=0; i < num_cores; i++)
        cores[i]->oracle->trace_in_flight_ops();
    ztrace_flush();

    if (assert_spin)
        while(1);
}

}  // xiosim::libsim
}  // xiosim
