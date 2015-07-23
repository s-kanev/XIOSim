/*
 * Exports called by instruction feeder.
 * Main entry point for simulated instructions.
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
#include "interface.h"
#include "host.h"
#include "misc.h"
#include "machine.h"
#include "endian.h"
#include "stats.h"
#include "sim.h"
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

#include "zesto-repeater.h"

extern void sim_main_slave_pre_pin(int coreID);
extern void sim_main_slave_pre_pin();
extern void sim_main_slave_post_pin(int coreID);
extern void sim_main_slave_post_pin(void);
extern bool sim_main_slave_fetch_insn(int coreID);

/* libconfuse options database */
extern cfg_t* all_opts;

/* stats database */
extern struct stat_sdb_t* sim_sdb;

/* power stats database */
extern struct stat_sdb_t* rtp_sdb;

/* redirected program/simulator output file names */
extern const char* sim_simout;

/* random number generator seed */
extern int rand_seed;

extern bool sim_slave_running;

extern void sim_print_stats(FILE* fd);

extern void start_slice(unsigned int slice_num);
extern void end_slice(unsigned int slice_num,
                      unsigned long long slice_length,
                      unsigned long long slice_weight_times_1000);
extern void scale_all_slices(void);

extern int min_coreID;

int Zesto_SlaveInit(int argc, char** argv) {
    char* s;
    int i;

    /* register an error handler */
    fatal_hook(sim_print_stats);

    /* Parse Zesto configuration file. This will populate both knobs and all_opts. */
    // TODO: Get rid of c-style cast.
    read_config_file(argc, (const char**)argv, &knobs);

    /* redirect I/O? */
    if (sim_simout != NULL) {
        /* send simulator non-interactive output (STDERR) to file SIM_SIMOUT */
        fflush(stderr);
        if (!freopen(sim_simout, "w", stderr))
            fatal("unable to redirect simulator output to file `%s'", sim_simout);
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

    /* initialize all simulation modules */
    sim_post_init();

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

    if (cores[0]->knobs->power.compute)
        init_power();

    sim_slave_running = true;

    // XXX: Zeroth cycle pre_pin missing in parallel version here. There's a good chance we don't
    // need it though.
    /* return control to Pin and wait for first instruction */
    return 0;
}

void Zesto_Destroy() {
    sim_slave_running = false;

    /* scale stats if running multiple simulation slices */
    scale_all_slices();

    /* print simulator stats */
    sim_print_stats(stderr);
    if (cores[0]->knobs->power.compute) {
        stat_save_stats(sim_sdb);
        compute_power(sim_sdb, true);
        deinit_power();
    }

    repeater_shutdown(cores[0]->knobs->exec.repeater_opt_str);

    /* If captured, print out ztrace */
    for (int i = 0; i < num_cores; i++)
        cores[i]->oracle->trace_in_flight_ops();
    ztrace_flush();

    for (int i = 0; i < num_cores; i++)
        if (cores[i]->stat.oracle_unknown_insn / (double)cores[i]->stat.oracle_total_insn > 0.02)
            fprintf(stderr,
                    "WARNING: [%d] More than 2%% instructions turned to NOPs (%lld out of %lld)\n",
                    i,
                    cores[i]->stat.oracle_unknown_insn,
                    cores[i]->stat.oracle_total_insn);

    // Free memory allocated by libconfuse for the configuration options.
    cfg_free(all_opts);
}

void deactivate_core(int coreID) {
    assert(coreID >= 0 && coreID < num_cores);
    ZTRACE_PRINT(coreID, "deactivate %d\n", coreID);
    lk_lock(&cycle_lock, coreID + 1);
    cores[coreID]->current_thread->active = false;
    cores[coreID]->current_thread->last_active_cycle = cores[coreID]->sim_cycle;
    int i;
    for (i = 0; i < num_cores; i++)
        if (cores[i]->current_thread->active) {
            min_coreID = i;
            break;
        }
    if (i == num_cores)
        min_coreID = MAX_CORES;
    lk_unlock(&cycle_lock);
}

void activate_core(int coreID) {
    assert(coreID >= 0 && coreID < num_cores);
    ZTRACE_PRINT(coreID, "activate %d\n", coreID);
    lk_lock(&cycle_lock, coreID + 1);
    cores[coreID]->current_thread->finished_cycle = false;  // Make sure master core will wait
    cores[coreID]->exec->update_last_completed(cores[coreID]->sim_cycle);
    cores[coreID]->current_thread->active = true;
    if (coreID < min_coreID)
        min_coreID = coreID;
    lk_unlock(&cycle_lock);
}

bool is_core_active(int coreID) {
    assert(coreID >= 0 && coreID < num_cores);
    bool result;
    lk_lock(&cycle_lock, coreID + 1);
    result = cores[coreID]->current_thread->active;
    lk_unlock(&cycle_lock);
    return result;
}

void sim_drain_pipe(int coreID) {
    struct core_t* core = cores[coreID];

    /* Just flush anything left */
    core->oracle->complete_flush();
    core->commit->recover();
    core->exec->recover();
    core->alloc->recover();
    core->decode->recover();
    core->fetch->recover(core->fetch->feeder_NPC);

    if (core->memory.mem_repeater)
        core->memory.mem_repeater->flush(core->current_thread->asid, NULL);
}

void Zesto_Slice_Start(unsigned int slice_num) { start_slice(slice_num); }

void Zesto_Slice_End(unsigned int slice_num,
                     unsigned long long feeder_slice_length,
                     unsigned long long slice_weight_times_1000) {
    // Record stats values
    end_slice(slice_num, feeder_slice_length, slice_weight_times_1000);
}

void Zesto_Resume(int coreID, handshake_container_t* handshake) {
    assert(coreID >= 0 && coreID < num_cores);
    struct core_t* core = cores[coreID];
    thread_t* thread = core->current_thread;
    bool slice_start = handshake->flags.isFirstInsn;

    if (!thread->active && !(slice_start || handshake->flags.flush_pipe)) {
        fprintf(stderr,
                "DEBUG DEBUG: Start/stop out of sync? %d PC: %x\n",
                coreID,
                handshake->pc);
        return;
    }

    zesto_assert(core->oracle->num_Mops_before_feeder() == 0, (void)0);
    zesto_assert(!core->oracle->spec_mode, (void)0);

    if (handshake->flags.flush_pipe) {
        sim_drain_pipe(coreID);
        return;
    }

    if (slice_start) {
        core->fetch->PC = handshake->pc;
    }

    // Let the oracle grab any arch state it needs
    core->oracle->grab_feeder_state(handshake, true, !slice_start);

    thread->fetches_since_feeder = 0;
    md_addr_t NPC = handshake->flags.brtaken ? handshake->tpc : handshake->npc;
    ZTRACE_PRINT(coreID, "PIN -> PC: %x, NPC: %x \n", handshake->pc, NPC);

    do {
        thread->consumed = false;
        bool fetch_more = sim_main_slave_fetch_insn(coreID);
        thread->fetches_since_feeder++;

        /* We can fetch more Mops this cycle, and oracle has them. */
        while (fetch_more && (core->oracle->num_Mops_before_feeder() > 0)) {
            thread->consumed = false;
            fetch_more = sim_main_slave_fetch_insn(coreID);
            thread->fetches_since_feeder++;
        }

        /* We can fetch more Mops this cycle, and oracle doesn't have them.
         * We'll get a new one from the feeder, once we re-enter. */
        if (fetch_more)
            break;

        /* Ok, we can't fetch more, wrap this cycle up. */
        sim_main_slave_post_pin(coreID);

        /* This is already next cycle, up to fetch. */
        sim_main_slave_pre_pin(coreID);
        /* Stay in the loop until the oracle is done with the requested Mop. */
    } while (!thread->consumed);

    zesto_assert(core->fetch->PC == NPC, (void)0);
}

void Zesto_WarmLLC(int asid, unsigned int addr, bool is_write) {
    struct core_t* core = cores[0];

    enum cache_command cmd = is_write ? CACHE_WRITE : CACHE_READ;
    md_paddr_t paddr = xiosim::memory::v2p_translate(asid, addr);
    if (!cache_is_hit(uncore->LLC, cmd, paddr, core)) {
        struct cache_line_t* p = cache_get_evictee(uncore->LLC, paddr, core);
        p->dirty = p->valid = false;
        cache_insert_block(uncore->LLC, cmd, paddr, core);
    }
}
