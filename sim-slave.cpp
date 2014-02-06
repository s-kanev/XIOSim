/* main.c - main line routines */
/*
 * Copyright © 2009 by Gabriel H. Loh and the Georgia Tech Research Corporation
 * Atlanta, GA  30332-0415
 * All Rights Reserved.
 * 
 * THIS IS A LEGAL DOCUMENT BY DOWNLOADING ZESTO, YOU ARE AGREEING TO THESE
 * TERMS AND CONDITIONS.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * NOTE: Portions of this release are directly derived from the SimpleScalar
 * Toolset (property of SimpleScalar LLC), and as such, those portions are
 * bound by the corresponding legal terms and conditions.  All source files
 * derived directly or in part from the SimpleScalar Toolset bear the original
 * user agreement.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Georgia Tech Research Corporation nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * 4. Zesto is distributed freely for commercial and non-commercial use.  Note,
 * however, that the portions derived from the SimpleScalar Toolset are bound
 * by the terms and agreements set forth by SimpleScalar, LLC.  In particular:
 * 
 *   "Nonprofit and noncommercial use is encouraged. SimpleScalar may be
 *   downloaded, compiled, executed, copied, and modified solely for nonprofit,
 *   educational, noncommercial research, and noncommercial scholarship
 *   purposes provided that this notice in its entirety accompanies all copies.
 *   Copies of the modified software can be delivered to persons who use it
 *   solely for nonprofit, educational, noncommercial research, and
 *   noncommercial scholarship purposes provided that this notice in its
 *   entirety accompanies all copies."
 * 
 * User is responsible for reading and adhering to the terms set forth by
 * SimpleScalar, LLC where appropriate.
 * 
 * 5. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 * 
 * 6. Noncommercial and nonprofit users may distribute copies of Zesto in
 * compiled or executable form as set forth in Section 2, provided that either:
 * (A) it is accompanied by the corresponding machine-readable source code, or
 * (B) it is accompanied by a written offer, with no time limit, to give anyone
 * a machine-readable copy of the corresponding source code in return for
 * reimbursement of the cost of distribution. This written offer must permit
 * verbatim duplication by anyone, or (C) it is distributed by someone who
 * received only the executable form, and is accompanied by a copy of the
 * written offer of source code.
 * 
 * 7. Zesto was developed by Gabriel H. Loh, Ph.D.  US Mail: 266 Ferst Drive,
 * Georgia Institute of Technology, Atlanta, GA 30332-0765
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/io.h>
#include <stdint.h>

#include "host.h"
#include "machine.h"
#include "misc.h"
#include "endian.h"
#include "version.h"
#include "options.h"
#include "stats.h"
#include "sim.h"

#include "zesto-cache.h"
#include "zesto-repeater.h"
#include "zesto-opts.h"
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

#include "interface.h"
#include "synchronization.h"

extern bool *consumers_sleep;
extern int *num_processes;
extern int heartbeat_count;
/* power stats database */
extern struct stat_sdb_t *rtp_sdb;

extern double LLC_speed;

/* architected state */
struct thread_t ** threads = NULL;

/* microarchitecture state */
struct core_t ** cores = NULL;

/* microarchitecture configuration parameters/knobs */
struct core_knobs_t knobs;

/* number of cores */
int num_cores = 1;

bool sim_slave_running = false;

/* Minimum ID of an active core. Used to simplify synchronization. */
int min_coreID;

/* Time between synchronizing a core and global state */
double sync_interval;

int heartbeat_count = 0;
int deadlock_count = 0;

/* initialize simulator data structures - called before any command-line options have been parsed! */
void
sim_pre_init(void)
{
  /* this only sets (malloc) up default values for the knobs */

  memzero(&knobs,sizeof(knobs));

  /* set default parameters */
  knobs.model = "DPM";

  knobs.memory.IL1PF_opt_str[0] = "nextline";
  knobs.memory.IL1_num_PF = 1;

  knobs.fetch.byteQ_size = 4;
  knobs.fetch.byteQ_linesize = 16;
  knobs.fetch.depth = 2;
  knobs.fetch.width = 4;
  knobs.fetch.IQ_size = 8;

  knobs.fetch.bpred_opt_str[0] = "2lev:gshare:1:1024:6:1";
  knobs.fetch.num_bpred_components = 1;

  knobs.decode.depth = 3;
  knobs.decode.width = 4;
  knobs.decode.target_stage = 1;
  knobs.decode.branch_decode_limit = 1;

  knobs.decode.decoders[0] = 4;
  for(int i=1;i<MAX_DECODE_WIDTH;i++)
    knobs.decode.decoders[i] = 1;
  knobs.decode.num_decoder_specs = 4;

  knobs.decode.MS_latency = 0;

  knobs.decode.uopQ_size = 8;

  knobs.alloc.depth = 2;
  knobs.alloc.width = 4;

  knobs.exec.RS_size = 20;
  knobs.exec.LDQ_size = 20;
  knobs.exec.STQ_size = 16;

  knobs.exec.num_exec_ports = 4;
  knobs.exec.payload_depth = 1;
  knobs.exec.fp_penalty = 0;

  knobs.exec.port_binding[FU_IEU].num_FUs = 2;
  knobs.exec.fu_bindings[FU_IEU][0] = 0;
  knobs.exec.fu_bindings[FU_IEU][1] = 1;
  knobs.exec.latency[FU_IEU] = 1;
  knobs.exec.issue_rate[FU_IEU] = 1;

  knobs.exec.port_binding[FU_JEU].num_FUs = 1;
  knobs.exec.fu_bindings[FU_JEU][0] = 0;
  knobs.exec.latency[FU_JEU] = 1;
  knobs.exec.issue_rate[FU_JEU] = 1;

  knobs.exec.port_binding[FU_IMUL].num_FUs = 1;
  knobs.exec.fu_bindings[FU_IMUL][0] = 2;
  knobs.exec.latency[FU_IMUL] = 4;
  knobs.exec.issue_rate[FU_IMUL] = 1;

  knobs.exec.port_binding[FU_SHIFT].num_FUs = 1;
  knobs.exec.fu_bindings[FU_SHIFT][0] = 0;
  knobs.exec.latency[FU_SHIFT] = 1;
  knobs.exec.issue_rate[FU_SHIFT] = 1;

  knobs.exec.port_binding[FU_FADD].num_FUs = 1;
  knobs.exec.fu_bindings[FU_FADD][0] = 0;
  knobs.exec.latency[FU_FADD] = 3;
  knobs.exec.issue_rate[FU_FADD] = 1;

  knobs.exec.port_binding[FU_FMUL].num_FUs = 1;
  knobs.exec.fu_bindings[FU_FMUL][0] = 1;
  knobs.exec.latency[FU_FMUL] = 5;
  knobs.exec.issue_rate[FU_FMUL] = 2;

  knobs.exec.port_binding[FU_FCPLX].num_FUs = 1;
  knobs.exec.fu_bindings[FU_FCPLX][0] = 2;
  knobs.exec.latency[FU_FCPLX] = 58;
  knobs.exec.issue_rate[FU_FCPLX] = 58;

  knobs.exec.port_binding[FU_IDIV].num_FUs = 1;
  knobs.exec.fu_bindings[FU_IDIV][0] = 2;
  knobs.exec.latency[FU_IDIV] = 13;
  knobs.exec.issue_rate[FU_IDIV] = 13;

  knobs.exec.port_binding[FU_FDIV].num_FUs = 1;
  knobs.exec.fu_bindings[FU_FDIV][0] = 2;
  knobs.exec.latency[FU_FDIV] = 32;
  knobs.exec.issue_rate[FU_FDIV] = 24;

  knobs.exec.port_binding[FU_LD].num_FUs = 1;
  knobs.exec.fu_bindings[FU_LD][0] = 1;
  knobs.exec.latency[FU_LD] = 1;
  knobs.exec.issue_rate[FU_LD] = 1;

  knobs.exec.port_binding[FU_STA].num_FUs = 1;
  knobs.exec.fu_bindings[FU_STA][0] = 2;
  knobs.exec.latency[FU_STA] = 1;
  knobs.exec.issue_rate[FU_STA] = 1;

  knobs.exec.port_binding[FU_STD].num_FUs = 1;
  knobs.exec.fu_bindings[FU_STD][0] = 3;
  knobs.exec.latency[FU_STD] = 1;
  knobs.exec.issue_rate[FU_STD] = 1;

  knobs.memory.DL2PF_opt_str[0] = "nextline";
  knobs.memory.DL2_num_PF = 1;
  knobs.memory.DL2_MSHR_cmd = "RPWB";

  knobs.memory.DL1PF_opt_str[0] = "nextline";
  knobs.memory.DL1_num_PF = 1;
  knobs.memory.DL1_MSHR_cmd = "RWBP";

  knobs.commit.ROB_size = 64;
  knobs.commit.width = 4;
}

/* initialize per-thread state, core state, etc. - called AFTER command-line parameters have been parsed */
void
sim_post_init(void)
{
  int i;
  assert(num_cores > 0);

  /* Initialize synchronization primitives */
  lk_init(&cycle_lock);
  lk_init(&memory_lock);
  lk_init(&cache_lock);

  /* initialize architected state(s) */
  threads = (struct thread_t **)calloc(num_cores,sizeof(*threads));
  if(!threads)
    fatal("failed to calloc threads");

  /* Initialize virtual memory */
  mem_init(*num_processes);

  for(i=0;i<num_cores;i++)
  {
    threads[i] = (struct thread_t *)calloc(1,sizeof(**threads));
    if(!threads[i])
      fatal("failed to calloc threads[%d]",i);

    threads[i]->id = i;

    threads[i]->finished_cycle = false;
    threads[i]->consumed = false;
    threads[i]->fetches_since_feeder = 0;
  }

  /* initialize microarchitecture state */
  cores = (struct core_t**) calloc(num_cores,sizeof(*cores));
  if(!cores)
    fatal("failed to calloc cores");
  for(i=0;i<num_cores;i++)
  {
    cores[i] = new core_t(i);
    if(!cores[i])
      fatal("failed to calloc cores[]");

    cores[i]->current_thread = threads[i];
    cores[i]->knobs = &knobs;
  }

  // Needs to be called before creating core->exec
  repeater_init(knobs.exec.repeater_opt_str);

  for(i=0;i<num_cores;i++)
  {
    cores[i]->oracle  = new core_oracle_t(cores[i]);
    cores[i]->commit  = commit_create(knobs.model,cores[i]);
    cores[i]->exec  = exec_create(knobs.model,cores[i]);
    cores[i]->alloc  = alloc_create(knobs.model,cores[i]);
    cores[i]->decode  = decode_create(knobs.model,cores[i]);
    cores[i]->fetch  = fetch_create(knobs.model,cores[i]);
    cores[i]->power = power_create(knobs.model,cores[i]);
    cores[i]->vf_controller = vf_controller_create(knobs.dvfs_opt_str,cores[i]);
  }

  // Time between updating global state (uncore, different nocs)
  sync_interval = MIN(1e-3 / LLC_speed, 1e-3 / cores[0]->memory.mem_repeater->speed);

  min_coreID = 0;
}

//Returns true if another instruction can be fetched in the same cycle
bool sim_main_slave_fetch_insn(int coreID)
{
    return cores[coreID]->fetch->do_fetch();
}

static void global_step(void)
{
    static int repeater_noc_ticks = 0;
    double uncore_ratio = cores[0]->memory.mem_repeater->speed / LLC_speed;
    // XXX: Assume repeater NoC running at a multiple of the uncore clock
    // (effectively no DFS when we have a repeater)
    // This should get fixed once we clock the repeater network separately.
    assert(uncore_ratio - floor(uncore_ratio) == 0.0);
    if (uncore_ratio > 0)
      repeater_noc_ticks = modinc(repeater_noc_ticks, (int)uncore_ratio);

    if(repeater_noc_ticks == 0) {
      /* Heartbeat -> print that the simulator is still alive */
      if((heartbeat_frequency > 0) && (heartbeat_count >= heartbeat_frequency))
      {
        lk_lock(printing_lock, 1);
        fprintf(stderr,"##HEARTBEAT## %lld: {",uncore->sim_cycle);
        long long int sum = 0;
        for(int i=0;i<num_cores;i++)
        {
          sum += cores[i]->stat.commit_insn;
          if(i < (num_cores-1))
            fprintf(stderr,"%lld, ",cores[i]->stat.commit_insn);
          else
            fprintf(stderr,"%lld, all=%lld}\n",cores[i]->stat.commit_insn, sum);
        }
        fflush(stderr);
        lk_unlock(printing_lock);
        heartbeat_count = 0;
      }

      /* Global deadlock detection -> kill simulation if no core is making progress */
      if((core_commit_t::deadlock_threshold > 0) && (deadlock_count >= core_commit_t::deadlock_threshold))
      {
        bool deadlocked = true;
        for(int i=0;i<num_cores;i++)
        {
            if (!cores[i]->current_thread->active)
                continue;
            deadlocked &= cores[i]->commit->deadlocked;
        }

        if(deadlocked) {
            core_t * core = cores[0];
            zesto_assert(false, (void)0);
        }

        deadlock_count = 0;
      }

      ZPIN_TRACE(INVALID_CORE, "###Uncore cycle%s\n"," ");

      if(uncore->sim_cycle == 0)
        fprintf(stderr, "### starting timing simulation \n");

      uncore->sim_cycle++;
      uncore->sim_time = uncore->sim_cycle / LLC_speed;
      uncore->default_cpu_cycles = (tick_t)ceil((double)uncore->sim_cycle * knobs.default_cpu_speed / LLC_speed);

      /* power computation */
      if(knobs.power.compute && (knobs.power.rtp_interval > 0) && 
         (uncore->sim_cycle % knobs.power.rtp_interval == 0))
      {
        stat_save_stats_delta(rtp_sdb);   // Store delta values for translation
        compute_power(rtp_sdb, false);
        stat_save_stats(rtp_sdb);         // Create new checkpoint for next delta
      } 

      if(knobs.dvfs_interval > 0)
        for(int i=0; i<num_cores; i++)
          if(cores[i]->sim_cycle >= cores[i]->vf_controller->next_invocation)
          {
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
    for(int i=0;i<num_cores;i++)
      if(cores[i]->memory.mem_repeater)
        cores[i]->memory.mem_repeater->step();
}

void sim_main_slave_pre_pin(int coreID)
{
  volatile int cores_finished_cycle = 0;
  volatile int cores_active = 0;

  if(cores[coreID]->current_thread->active) {
    cores[coreID]->stat.final_sim_cycle = cores[coreID]->sim_cycle;
    // Finally time to step local cycle counter
    cores[coreID]->sim_cycle++;

    cores[coreID]->ns_passed += 1e-3 / cores[coreID]->cpu_speed;
  }

  /* Time to sync with uncore */
  if(cores[coreID]->ns_passed >= sync_interval) {
    cores[coreID]->ns_passed = 0.0;

    /* Thread is joining in serial region. Mark it as finished this cycle */
    /* Spin if serial region stil hasn't finished
     * XXX: SK: Is this just me being paranoid? After all, serial region
     * updates finished_cycle atomically for all threads and none can
     * race through to here before that update is finished.
     */
    lk_lock(&cycle_lock, coreID+1);
    cores[coreID]->current_thread->finished_cycle = true;

    /* Active core with smallest id -- Wait for all cores to be finished and
       update global state */
    if (coreID == min_coreID)
    {
      do {
master_core:
        /* Re-check if all cores finished this cycle. */
        cores_finished_cycle = 0;
        cores_active = 0;
        for(int i=0; i<num_cores; i++) {
          if(cores[i]->current_thread->finished_cycle)
            cores_finished_cycle++;
          if(cores[i]->current_thread->active)
            cores_active++;
        }

        /* Yeah, could be >, see StopSimulation in feeder_zesto.C */
        if (cores_finished_cycle >= cores_active)
          break;

        lk_unlock(&cycle_lock);
        /* Spin, spin, spin */
        yield();
        lk_wait_consumers();
        lk_lock(&cycle_lock, coreID+1);

        if (coreID != min_coreID)
          goto non_master_core;
      } while(true); 

      /* Process shared state once all cores are gathered here. */
      global_step();

      /* HACKEDY HACKEDY HACK */
      /* Non-active cores should still step their private caches because there might
       * be accesses scheduled there from the repeater network */
      /* XXX: This is round-robin for LLC based on core id, if that matters */
      for(int i=0; i<num_cores; i++)
        if(!cores[i]->current_thread->active) {
          if(cores[i]->memory.DL1->check_for_work)
            cache_process(cores[i]->memory.DL1);
          if(cores[i]->memory.DL2 && cores[i]->memory.DL2->check_for_work)
            cache_process(cores[i]->memory.DL2);
      }

      /* Unblock other cores to keep crunching. */
      for(int i=0; i<num_cores; i++)
        cores[i]->current_thread->finished_cycle = false; 
      lk_unlock(&cycle_lock);
    }
    /* All other cores -- spin until global state update is finished */
    else
    {
      while(cores[coreID]->current_thread->finished_cycle) {
        if (coreID == min_coreID) 
          /* If we become the "master core", make sure everyone is at critical section. */
          goto master_core;

        /* All cores got deactivated, just return and make sure we 
         * go back to PIN */
        if (min_coreID == MAX_CORES) {
          ZPIN_TRACE(min_coreID, "Returning from step loop looking suspicious %d", coreID);
          cores[coreID]->current_thread->consumed = true;
          lk_unlock(&cycle_lock);
          return;
        }

non_master_core:
        /* Spin, spin, spin */
        lk_unlock(&cycle_lock);
        yield();
        lk_wait_consumers();
        lk_lock(&cycle_lock, coreID+1);
      }
      lk_unlock(&cycle_lock);
    }

  }

  step_core_PF_controllers(cores[coreID]);

  cores[coreID]->commit->IO_step(); /* IO cores only */ //UGLY UGLY UGLY

  /* all memory processed here */
  //XXX: RR
  cores[coreID]->exec->LDST_exec();

  cores[coreID]->commit->step();  /* OoO cores only */

  cores[coreID]->commit->pre_commit_step(); /* IO cores only */

  cores[coreID]->exec->step();      /* IO cores only */
  cores[coreID]->exec->ALU_exec();  /* OoO cores only */

  cores[coreID]->exec->LDQ_schedule();

  cores[coreID]->exec->RS_schedule();  /* OoO cores only */

  cores[coreID]->alloc->step();

  cores[coreID]->decode->step();

  /* round-robin on which cache to process first so that one core
     doesn't get continual priority over the others for L2 access */
  //XXX: RR
  cores[coreID]->fetch->post_fetch();
}

void sim_main_slave_post_pin(int coreID)
{
  /* round-robin on which cache to process first so that one core
     doesn't get continual priority over the others for L2 access */
  //XXX: RR
  cores[coreID]->fetch->pre_fetch();

  /* this is done last in the cycle so that prefetch requests have the
     lowest priority when competing for queues, buffers, etc. */
  if(coreID == min_coreID)
  {
    lk_lock(&cache_lock, coreID+1);
    prefetch_LLC(uncore);
    lk_unlock(&cache_lock);
  }

  /* process prefetch requests in reverse order as L1/L2; i.e., whoever
     got the lowest priority for L1/L2 processing gets highest priority
     for prefetch processing */
  //XXX: RR
  prefetch_core_caches(cores[coreID]);

  /*******************/
  /* occupancy stats */
  /*******************/
  /* this avoids the need to guard each stat update below with "ZESTO_STAT()" */
  if(cores[coreID]->current_thread->active)
  {
    cores[coreID]->oracle->update_occupancy();
    cores[coreID]->fetch->update_occupancy();
    cores[coreID]->decode->update_occupancy();
    cores[coreID]->exec->update_occupancy();
    cores[coreID]->commit->update_occupancy();
  }

  /* check to see if all cores are "ok" */
  if(cores[coreID]->oracle->hosed)
    fatal("Core %d got hosed, quitting."); 
}
