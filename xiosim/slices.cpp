#include <stdio.h>
#include <limits.h>
#include <cmath>
#include <vector>

#include "misc.h"
#include "synchronization.h"
#include "stats.h"
#include "sim.h"
#include "zesto-core.h"
#include "zesto-power.h"
#include "zesto-uncore.h"
#include "zesto-structs.h"

#include "slices.h"

// XXX: THIS STILL DOESN'T HANDLE MULTICORE SLICES PROPERLY.
// FORTUNATELY WE ONLY SIMULATE SINGLE MULTICORE SLICES SO FAR.

/* Global members from sim.cpp that we'll overwrite between slices. */
extern xiosim::stats::StatsDatabase* sim_sdb;
extern int sim_elapsed_time;

static unsigned long long slice_core_start_cycle = 0;
static unsigned long long slice_uncore_start_cycle = 0;
static unsigned long long slice_core_end_cycle = 0;
static unsigned long long slice_uncore_end_cycle = 0;
static unsigned long long slice_start_icount = 0;

static time_t slice_start_time;

static std::vector<xiosim::stats::StatsDatabase*> all_stats;

void start_slice(unsigned int slice_num) {
    int i = 0;
    core_t* core = cores[i];

    /* create stats database for this slice */
    xiosim::stats::StatsDatabase* new_stat_db = stat_new();

    /* register new database with stat counters */
    xiosim::libsim::sim_reg_stats(new_stat_db);

    all_stats.push_back(new_stat_db);

    uncore->sim_cycle = slice_uncore_end_cycle;
    cores[i]->sim_cycle = slice_core_end_cycle;
    cores[i]->stat.final_sim_cycle = slice_core_end_cycle;
    slice_core_start_cycle = cores[i]->sim_cycle;
    slice_uncore_start_cycle = uncore->sim_cycle;
    slice_start_icount = core->stat.oracle_num_insn;
    slice_start_time = time((time_t*)NULL);
}

void end_slice(unsigned int slice_num,
               unsigned long long feeder_slice_length,
               unsigned long long slice_weight_times_1000) {
    int i = 0;
    xiosim::stats::StatsDatabase* curr_sdb = all_stats.back();
    slice_uncore_end_cycle = uncore->sim_cycle;
    slice_core_end_cycle = cores[i]->sim_cycle;
    time_t slice_end_time = time((time_t*)NULL);
    sim_elapsed_time = std::max(slice_end_time - slice_start_time, (time_t)1);

    /* Ugh, this feels very dirty. Have to make sure we don't forget a cycle stat.
       The reason for doing this is that cycle counts increasing monotonously is
       an important invariant all around and reseting the cycle counts on every
       slice causes hell to break loose with caches an you name it */
    uncore->sim_cycle -= slice_uncore_start_cycle;
    cores[i]->sim_cycle -= slice_core_start_cycle;
    cores[i]->stat.final_sim_cycle -= slice_core_start_cycle;
    double weight = (double)slice_weight_times_1000 / 100000.0;
    curr_sdb->slice_weight = weight;

    unsigned long long slice_length = cores[i]->stat.oracle_num_insn - slice_start_icount;
    // Check if simulator and feeder measure instruction counts in the same way
    // (they may not count REP-s the same, f.e.)
    double slice_length_diff = 1.0 - ((double)slice_length / (double)feeder_slice_length);
    if ((fabs(slice_length_diff) > 0.01) && (feeder_slice_length > 0)) {
        lk_lock(printing_lock, 1);
        fprintf(stderr,
                "Significant slice length different between sim and feeder! Slice: %u, sim_length: "
                "%llu, feeder_length: %llu\n",
                slice_num,
                slice_length,
                feeder_slice_length);
        lk_unlock(printing_lock);
    }

    stat_save_stats(curr_sdb);
    /* Print slice stats separately */
    if (/*verbose && */ system_knobs.sim_simout != NULL) {
        char curr_filename[PATH_MAX];
        sprintf(curr_filename, "%s.slice.%d", system_knobs.sim_simout, slice_num);
        FILE* curr_fd = freopen(curr_filename, "w", stderr);
        if (curr_fd != NULL) {
            stat_print_stats(curr_sdb, stderr);
            if (system_knobs.power.compute)
                compute_power(curr_sdb, true);
            fclose(curr_fd);
        }
        // Restore stderr redirection
        if (system_knobs.sim_simout)
            curr_fd = freopen(system_knobs.sim_simout, "a", stderr);
        else
            curr_fd = fopen("/dev/tty", "a");
        if (curr_fd == NULL)
            fatal("couldn't restore stderr redirection");
    }

    double n_cycles = (double)cores[0]->sim_cycle;
    double n_insn = (double)(cores[0]->stat.oracle_num_insn - slice_start_icount);
    double n_pin_n_insn = (double)feeder_slice_length;
    double curr_cpi = weight * n_cycles / n_insn;
    double curr_ipc = 1.0 / curr_cpi;

    lk_lock(printing_lock, 1);
    fprintf(stderr,
            "Slice %d, weight: %.4f, IPC/weight: %.2f, n_insn: %.0f, n_insn_pin: %.0f, n_cycles: "
            "%.0f\n",
            slice_num,
            weight,
            curr_ipc,
            n_insn,
            n_pin_n_insn,
            n_cycles);
    lk_unlock(printing_lock);
}

void scale_all_slices(void) {
    int n_slices = all_stats.size();

    /* No active slices */
    if (n_slices == 0)
        return;

    /* create stats database for averages */
    xiosim::stats::StatsDatabase* avg_stat_db = stat_new();

    /* register new database with stat counters */
    xiosim::libsim::sim_reg_stats(avg_stat_db);

    /* clean up old global DB */
    /* TODO(skanev): this gets a bit ugly (as anything in this file). Clean up! */
    delete sim_sdb;
    /* ... and set it as the default that gets output */
    sim_sdb = avg_stat_db;

    /* Now, scale all stats by the slice weight and accumulate statistics across
     * all slices. Note that this is a destructive operation on the existing
     * StatsDatabases.
     */
    for (auto it = all_stats.begin(); it != all_stats.end(); ++it) {
        xiosim::stats::StatsDatabase* curr_sdb = *it;
        curr_sdb->scale_all_stats();
        sim_sdb->accum_all_stats(curr_sdb);
    }

    /* clean up slice DBs */
    for (auto sdb : all_stats) {
        delete sdb;
    }
}
