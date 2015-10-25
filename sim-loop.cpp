/* Sim-loop -- libsim's main event loop */

#include <assert.h>
#include <cmath>

#include "host.h"
#include "misc.h"
#include "memory.h"
#include "stats.h"
#include "sim.h"

#include "zesto-structs.h"
#include "zesto-cache.h"
#include "zesto-repeater.h"
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

#include "synchronization.h"
#include "libsim.h"

extern void CheckIPCMessageQueue(bool isEarly, int caller_coreID);

extern double LLC_speed;

int heartbeat_frequency = 0;

namespace xiosim {
namespace libsim {

/* Minimum ID of an active core. Used to simplify synchronization. */
static int min_coreID = 0;

/* Cycle lock is used to synchronize global state (cycle counter)
 * and not let a core advance in time without increasing cycle. */
static XIOSIM_LOCK cycle_lock;

/* Time between synchronizing a core and global state */
static double sync_interval;

static int heartbeat_count = 0;
static int deadlock_count = 0;

static void sim_drain_pipe(int coreID);

void sim_loop_init(void) {
    // Time between updating global state (uncore, different nocs)
    sync_interval = std::min(1e-3 / LLC_speed, 1e-3 / cores[0]->memory.mem_repeater->speed);
}

static void global_step(void) {
    static int repeater_noc_ticks = 0;
    double uncore_ratio = cores[0]->memory.mem_repeater->speed / LLC_speed;
    // XXX: Assume repeater NoC running at a multiple of the uncore clock
    // (effectively no DFS when we have a repeater)
    // This should get fixed once we clock the repeater network separately.
    assert(uncore_ratio - floor(uncore_ratio) == 0.0);
    if (uncore_ratio > 0)
        repeater_noc_ticks = modinc(repeater_noc_ticks, (int)uncore_ratio);

    if (repeater_noc_ticks == 0) {
        /* Heartbeat -> print that the simulator is still alive */
        if ((heartbeat_frequency > 0) && (heartbeat_count >= heartbeat_frequency)) {
            lk_lock(printing_lock, 1);
            fprintf(stderr, "##HEARTBEAT## %" PRId64": {", uncore->sim_cycle);
            counter_t sum = 0;
            for (int i = 0; i < num_cores; i++) {
                sum += cores[i]->stat.commit_insn;
                if (i < (num_cores - 1))
                    fprintf(stderr, "%" PRId64", ", cores[i]->stat.commit_insn);
                else
                    fprintf(stderr, "%" PRId64", all=%" PRId64"}\n", cores[i]->stat.commit_insn, sum);
            }
            fflush(stderr);
            lk_unlock(printing_lock);
            heartbeat_count = 0;
        }

        /* Global deadlock detection -> kill simulation if no core is making progress */
        if ((core_commit_t::deadlock_threshold > 0) &&
            (deadlock_count >= core_commit_t::deadlock_threshold)) {
            bool deadlocked = true;
            for (int i = 0; i < num_cores; i++) {
                if (!cores[i]->active)
                    continue;
                deadlocked &= cores[i]->commit->deadlocked;
            }

            if (deadlocked) {
                core_t* core = cores[0];
                zesto_assert(false, (void)0);
            }

            deadlock_count = 0;
        }

        ZTRACE_PRINT(INVALID_CORE, "###Uncore cycle%s\n", " ");

        if (uncore->sim_cycle == 0)
            fprintf(stderr, "### starting timing simulation \n");

        uncore->sim_cycle++;
        uncore->sim_time = uncore->sim_cycle / LLC_speed;
        uncore->default_cpu_cycles =
            (tick_t)ceil((double)uncore->sim_cycle * knobs.default_cpu_speed / LLC_speed);

        /* power computation */
        if (knobs.power.compute && (knobs.power.rtp_interval > 0) &&
            (uncore->sim_cycle % knobs.power.rtp_interval == 0)) {
            compute_rtp_power();
        }

        if (knobs.dvfs_interval > 0)
            for (int i = 0; i < num_cores; i++)
                if (cores[i]->sim_cycle >= cores[i]->vf_controller->next_invocation) {
                    cores[i]->vf_controller->change_vf();
                    cores[i]->vf_controller->next_invocation += knobs.dvfs_interval;
                }

        heartbeat_count++;
        deadlock_count++;

        /* Check for messages coming from producer processes
         * and execute accordingly */
        CheckIPCMessageQueue(false, min_coreID);

        /*********************************************/
        /* step through pipe stages in reverse order */
        /*********************************************/

        dram->refresh();
        uncore->MC->step();

        step_LLC_PF_controller(uncore);

        cache_process(uncore->LLC);
    }

    // Until we fix synchronization, this is global, and running at core freq.
    for (int i = 0; i < num_cores; i++)
        if (cores[i]->memory.mem_repeater)
            cores[i]->memory.mem_repeater->step();
}

// Returns true if another instruction can be fetched in the same cycle
static bool sim_main_slave_fetch_insn(int coreID) { return cores[coreID]->fetch->do_fetch(); }

static void sim_main_slave_pre_pin(int coreID) {
    volatile int cores_finished_cycle = 0;
    volatile int cores_active = 0;

    if (cores[coreID]->active) {
        cores[coreID]->stat.final_sim_cycle = cores[coreID]->sim_cycle;
        // Finally time to step local cycle counter
        cores[coreID]->sim_cycle++;

        cores[coreID]->ns_passed += 1e-3 / cores[coreID]->cpu_speed;
    }

    /* Time to sync with uncore */
    if (cores[coreID]->ns_passed >= sync_interval) {
        cores[coreID]->ns_passed = 0.0;

        /* Thread is joining in serial region. Mark it as finished this cycle */
        /* Spin if serial region stil hasn't finished
         * XXX: SK: Is this just me being paranoid? After all, serial region
         * updates finished_cycle atomically for all threads and none can
         * race through to here before that update is finished.
         */
        lk_lock(&cycle_lock, coreID + 1);
        cores[coreID]->finished_cycle = true;

        /* Active core with smallest id -- Wait for all cores to be finished and
           update global state */
        if (coreID == min_coreID) {
            do {
            master_core:
                /* Re-check if all cores finished this cycle. */
                cores_finished_cycle = 0;
                cores_active = 0;
                for (int i = 0; i < num_cores; i++) {
                    if (cores[i]->finished_cycle)
                        cores_finished_cycle++;
                    if (cores[i]->active)
                        cores_active++;
                }

                /* Yeah, could be >, see StopSimulation in feeder_zesto.C */
                if (cores_finished_cycle >= cores_active)
                    break;

                lk_unlock(&cycle_lock);
                /* Spin, spin, spin */
                yield();
                lk_lock(&cycle_lock, coreID + 1);

                if (coreID != min_coreID)
                    goto non_master_core;
            } while (true);

            /* Process shared state once all cores are gathered here. */
            global_step();

            /* HACKEDY HACKEDY HACK */
            /* Non-active cores should still step their private caches because there might
             * be accesses scheduled there from the repeater network */
            /* XXX: This is round-robin for LLC based on core id, if that matters */
            for (int i = 0; i < num_cores; i++) {
                if (!cores[i]->active) {
                    if (cores[i]->memory.DL2)
                        cache_process(cores[i]->memory.DL2);
                    cache_process(cores[i]->memory.DL1);
                }
            }

            /* Unblock other cores to keep crunching. */
            for (int i = 0; i < num_cores; i++)
                cores[i]->finished_cycle = false;
            lk_unlock(&cycle_lock);
        }
        /* All other cores -- spin until global state update is finished */
        else {
            while (cores[coreID]->finished_cycle) {
                if (coreID == min_coreID)
                    /* If we become the "master core", make sure everyone is at critical section. */
                    goto master_core;

                /* All cores got deactivated, just return and make sure we
                 * go back to PIN */
                if (min_coreID == MAX_CORES) {
                    ZTRACE_PRINT(
                        min_coreID, "Returning from step loop looking suspicious %d", coreID);
                    cores[coreID]->oracle->consumed = true;
                    lk_unlock(&cycle_lock);
                    return;
                }

            non_master_core:
                /* Spin, spin, spin */
                lk_unlock(&cycle_lock);
                yield();
                lk_lock(&cycle_lock, coreID + 1);
            }
            lk_unlock(&cycle_lock);
        }
    }

    step_core_PF_controllers(cores[coreID]);

    cores[coreID]->commit->IO_step(); /* IO cores only */  // UGLY UGLY UGLY

    /* all memory processed here */
    // XXX: RR
    cores[coreID]->exec->LDST_exec();

    cores[coreID]->commit->step(); /* OoO cores only */

    cores[coreID]->commit->pre_commit_step(); /* IO cores only */

    cores[coreID]->exec->step();     /* IO cores only */
    cores[coreID]->exec->ALU_exec(); /* OoO cores only */

    cores[coreID]->exec->LDQ_schedule();

    cores[coreID]->exec->RS_schedule(); /* OoO cores only */

    cores[coreID]->alloc->step();

    cores[coreID]->decode->step();

    /* round-robin on which cache to process first so that one core
       doesn't get continual priority over the others for L2 access */
    // XXX: RR
    cores[coreID]->fetch->post_fetch();
}

void sim_main_slave_post_pin(int coreID) {
    /* round-robin on which cache to process first so that one core
       doesn't get continual priority over the others for L2 access */
    // XXX: RR
    cores[coreID]->fetch->pre_fetch();

    /* this is done last in the cycle so that prefetch requests have the
       lowest priority when competing for queues, buffers, etc. */
    if (coreID == min_coreID) {
        lk_lock(&cache_lock, coreID + 1);
        prefetch_LLC(uncore);
        lk_unlock(&cache_lock);
    }

    /* process prefetch requests in reverse order as L1/L2; i.e., whoever
       got the lowest priority for L1/L2 processing gets highest priority
       for prefetch processing */
    // XXX: RR
    prefetch_core_caches(cores[coreID]);

    /*******************/
    /* occupancy stats */
    /*******************/
    /* this avoids the need to guard each stat update below with "ZESTO_STAT()" */
    if (cores[coreID]->active) {
        cores[coreID]->oracle->update_occupancy();
        cores[coreID]->fetch->update_occupancy();
        cores[coreID]->decode->update_occupancy();
        cores[coreID]->exec->update_occupancy();
        cores[coreID]->commit->update_occupancy();
    }
}

void simulate_handshake(int coreID, handshake_container_t* handshake) {
    assert(coreID >= 0 && coreID < num_cores);
    struct core_t* core = cores[coreID];
    bool slice_start = handshake->flags.isFirstInsn;

    core->stat.feeder_handshakes++;

    if (!core->active && !(slice_start || handshake->flags.flush_pipe)) {
        fprintf(stderr, "DEBUG DEBUG: Start/stop out of sync? %d PC: %" PRIxPTR"\n", coreID, handshake->pc);
        return;
    }

    /* Make sure we've cleared recoveries before coming back to feeder for a hshake. */
    zesto_assert(!core->oracle->on_nuke_recovery_path(), (void)0);

    if (handshake->flags.flush_pipe) {
        sim_drain_pipe(coreID);
        return;
    }

    if (slice_start) {
        core->fetch->PC = handshake->pc;
        core->fetch->feeder_NPC = handshake->pc;
    }

#ifdef ZTRACE
    md_addr_t NPC = handshake->flags.brtaken ? handshake->tpc : handshake->npc;
#endif
    ZTRACE_PRINT(coreID,
                 "PIN -> PC: %" PRIxPTR", NPC: %" PRIxPTR" spec: %d\n",
                 handshake->pc,
                 NPC,
                 handshake->flags.speculative);

    buffer_result_t buffer_result = ALL_GOOD;
    bool nuke_recovery = false;

    /* Make sure we didn't get back to feeder before absorbing a hshake. */
    zesto_assert(core->oracle->consumed, (void)0);
    bool did_buffer = false;
    bool did_consume = false;

    do {
        nuke_recovery = core->oracle->on_nuke_recovery_path();

        bool fetch_more = false;
        /* Buffer the new one in the oracle shadow_MopQ. */
        /* We want to buffer if: */
        if (!did_buffer &&               // we haven't buffered it already
            !nuke_recovery &&            // we're not recovering from a nuke
            core->oracle->can_exec()) {  // oracle wants to and can absorb a new hshake
            buffer_result = core->oracle->buffer_handshake(handshake);
            did_buffer = (buffer_result == ALL_GOOD);
            if (buffer_result == HANDSHAKE_NOT_NEEDED) {
                core->oracle->consumed = true;
                return;
            }
        }

        /* Execute the oracle. If all is well, the handshake is consumed, and, depending
         * on fetch conditions (taken branches, fetch buffers), we either need a new one,
         * or can carry on simulating the cycle. */
        core->oracle->consumed = false;
        fetch_more = sim_main_slave_fetch_insn(coreID);
        did_consume = core->oracle->consumed;
        if (!did_consume)
            zesto_assert(!fetch_more, (void)0);

        /* We can fetch more Mops this cycle. */
        if (fetch_more) {
            /* On a nuke recovery, oracle has all the needed Mops in the shadow_MopQ.
             * So, just use them, don't go back to the feeder until we've recovered. */
            if (core->oracle->on_nuke_recovery_path()) {
                // XXX: Gather stats.
                continue;
            }

            /* We didn't process the handshake we're consuming now, stay in this loop
             * until we do.
             * This happens if the feeder didn't give us a speculative hshake, but we did
             * speculate, so we had to come up with a fake NOP. */
            if (buffer_result == HANDSHAKE_NOT_CONSUMED) {
                continue;
            }

            /* Oracle doesn't have the right handshake.
             * We'll get a new one from the feeder, once we re-enter. */
            zesto_assert(core->oracle->consumed, (void)0);
            return;
        }

        /* Ok, we can't fetch more, wrap this cycle up. */
        sim_main_slave_post_pin(coreID);

        /* This is already next cycle, up to fetch. */
        sim_main_slave_pre_pin(coreID);

        /* Re-check for nuke recoveries (they could happen here if jeclear_delay == 0). */
        nuke_recovery = core->oracle->on_nuke_recovery_path();
        /* Re-check that we've consumed last op. If jeclear_delay == 0, we could have
         * blown it away, which technically does consume it. */
        did_consume |= core->oracle->consumed;
        ZTRACE_PRINT(core->id,
                     "End of loop. Buffer result: %d, consumed: %d, before_feeder: %d\n",
                     buffer_result,
                     did_consume,
                     core->oracle->num_Mops_before_feeder());

        /* Stay in the loop until oracle needs a new handshake. */
    } while (buffer_result == HANDSHAKE_NOT_CONSUMED || !did_consume || nuke_recovery ||
             core->oracle->is_draining());
}

void deactivate_core(int coreID) {
    assert(coreID >= 0 && coreID < num_cores);
    ZTRACE_PRINT(coreID, "deactivate %d\n", coreID);
    lk_lock(&cycle_lock, coreID + 1);
    cores[coreID]->active = false;
    cores[coreID]->last_active_cycle = cores[coreID]->sim_cycle;
    int i;
    for (i = 0; i < num_cores; i++) {
        if (cores[i]->active) {
            min_coreID = i;
            break;
        }
    }
    if (i == num_cores)
        min_coreID = MAX_CORES;
    lk_unlock(&cycle_lock);
}

void activate_core(int coreID) {
    assert(coreID >= 0 && coreID < num_cores);
    ZTRACE_PRINT(coreID, "activate %d\n", coreID);
    lk_lock(&cycle_lock, coreID + 1);
    cores[coreID]->finished_cycle = false;  // Make sure master core will wait
    cores[coreID]->exec->update_last_completed(cores[coreID]->sim_cycle);
    cores[coreID]->active = true;
    if (coreID < min_coreID)
        min_coreID = coreID;
    lk_unlock(&cycle_lock);
}

bool is_core_active(int coreID) {
    assert(coreID >= 0 && coreID < num_cores);
    bool result;
    lk_lock(&cycle_lock, coreID + 1);
    result = cores[coreID]->active;
    lk_unlock(&cycle_lock);
    return result;
}

static void sim_drain_pipe(int coreID) {
    struct core_t* core = cores[coreID];

    /* Just flush anything left */
    core->oracle->complete_flush();
    core->commit->recover();
    core->exec->recover();
    core->alloc->recover();
    core->decode->recover();
    core->fetch->recover(core->fetch->feeder_NPC);

    if (core->memory.mem_repeater)
        core->memory.mem_repeater->flush(core->asid, NULL);
}

void simulate_warmup(int asid, md_addr_t addr, bool is_write) {
    struct core_t* core = cores[0];

    enum cache_command cmd = is_write ? CACHE_WRITE : CACHE_READ;
    md_paddr_t paddr = xiosim::memory::v2p_translate(asid, addr);
    if (!cache_is_hit(uncore->LLC, cmd, paddr, core)) {
        struct cache_line_t* p = cache_get_evictee(uncore->LLC, paddr, core);
        p->dirty = p->valid = false;
        cache_insert_block(uncore->LLC, cmd, paddr, core);
    }
}

}  // xiosim::libsim
}  // xiosim
