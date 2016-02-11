#ifndef ZESTO_CORE_INCLUDED
#define ZESTO_CORE_INCLUDED

/* zesto-core.h - Zesto core (single pipeline) class
 *
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
 * 4. Zesto is distributed freely for commercial and non-commercial use.
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
 *
 */

#include "knobs.h"
#include "stats.h"
#define ZESTO_STAT(x) {if(core->active) {x}}

struct uop_t;

/* state for the processor microarchitecture (per core) */
class core_t {

  friend class core_oracle_t;

  public:

  /* pointer to configuration information */
  struct core_knobs_t * knobs;

  int id; /* core-id */
  tick_t sim_cycle; /* core-specific cycle counter */
  double cpu_speed; /* current core frequency in MHz */

  /* Simulation flow members */
  bool active;              /* FALSE if this core is not executing */
  tick_t last_active_cycle; /* Last time this core was active */
  double ns_passed;         /* used to sync with uncore */
  bool finished_cycle;      /* Ready to advance to next cycle? */
  bool in_critical_section; /* Are we executing a HELIX sequential cut? */

  counter_t num_emergency_recoveries;
  int last_emergency_recovery_count; /* inst count at last recover to detect an unrecoverable situation */

  int asid; /* Address space id this core is currently simulating from. */

  /***************************/
  /* MICROARCHITECTURE STATE */
  /***************************/

  class core_oracle_t * oracle;
  class core_fetch_t * fetch;
  class core_decode_t * decode;
  class core_alloc_t * alloc;
  class core_exec_t * exec;
  class core_commit_t * commit;
  class core_power_t * power;

  struct core_memory_t {
    struct cache_t * IL1;
    struct cache_t * ITLB;

    struct cache_t * DL1;
    struct cache_t * DL2;
    struct bus_t * DL2_bus; /* connects DL1 to DL2 */
    struct cache_t * DTLB;
    struct cache_t * DTLB2;
    struct bus_t * DTLB_bus; /* connects DTLB to DTLB2 */

    class repeater_t * mem_repeater;
  } memory;

  class vf_controller_t * vf_controller;

  /**********************/
  /* VARIOUS STATISTICS */
  /**********************/

  struct core_stat_t {
    tick_t final_sim_cycle; /* number of cycles when inst-limit reached (for multi-core sims) */
    /* fetch stage */
    counter_t fetch_bytes;
    counter_t fetch_insn;
    counter_t fetch_uops;
    counter_t fetch_eff_uops;
    counter_t byteQ_occupancy;
    counter_t predecode_bytes;
    counter_t predecode_insn;
    counter_t predecode_uops;
    counter_t predecode_eff_uops;
    counter_t IQ_occupancy;
    counter_t IQ_full_cycles;
    counter_t IQ_empty_cycles;
    xiosim::stats::Distribution* fetch_stall;

    /* decode stage */
    counter_t target_resteers;
    counter_t phantom_resteers;
    counter_t override_pred_resteers;
    counter_t decode_insn;
    counter_t decode_uops; /* uops - a set of fused uops only count as a single effective uop */
    counter_t decode_eff_uops; /* counts individual uops (fused or not) */
    counter_t uopQ_occupancy;
    counter_t uopQ_eff_occupancy;
    counter_t uopQ_full_cycles;
    counter_t uopQ_empty_cycles;
    xiosim::stats::Distribution* decode_stall;

    /* alloc stage */
    counter_t alloc_insn;
    counter_t alloc_uops;
    counter_t alloc_eff_uops;
    counter_t regfile_reads;
    counter_t fp_regfile_reads;
    counter_t ROB_writes;
    xiosim::stats::Distribution* alloc_stall;

    /* exec stage */
    counter_t exec_uops_issued;
    counter_t exec_uops_replayed;
    counter_t exec_uops_snatched_back;
    counter_t RS_occupancy;
    counter_t RS_eff_occupancy;
    counter_t RS_full_cycles;
    counter_t RS_empty_cycles;
    counter_t num_jeclear;
    counter_t num_wp_jeclear; /* while in shadow of earlier mispred */
    counter_t load_nukes; /* pipe-flushes due to load-order violation */
    counter_t wp_load_nukes;
    counter_t DL1_load_split_accesses;
    counter_t int_FU_occupancy;
    counter_t fp_FU_occupancy;
    counter_t mul_FU_occupancy;

    /* commit stage */
    counter_t commit_bytes;
    counter_t commit_insn;
    counter_t commit_uops;
    counter_t commit_eff_uops;
    counter_t commit_fusions;
    counter_t commit_refs;
    counter_t commit_loads;
    counter_t commit_branches;
    counter_t commit_traps;
    counter_t commit_rep_insn;
    counter_t commit_rep_iterations;
    counter_t commit_rep_uops;
    counter_t commit_UROM_insn;
    counter_t commit_UROM_uops;
    counter_t commit_UROM_eff_uops;
    counter_t DL1_store_split_accesses;
    counter_t regfile_writes;
    counter_t fp_regfile_writes;

    /* occupancy stats */
    counter_t ROB_occupancy;
    counter_t ROB_eff_occupancy;
    counter_t ROB_full_cycles;
    counter_t ROB_empty_cycles;
    counter_t LDQ_occupancy;
    counter_t LDQ_full_cycles;
    counter_t LDQ_empty_cycles;
    counter_t STQ_occupancy;
    counter_t STQ_full_cycles;
    counter_t STQ_empty_cycles;

    xiosim::stats::Distribution* commit_stall;

    int flow_count;
    int eff_flow_count;
    xiosim::stats::Distribution* flow_histo;
    xiosim::stats::Distribution* eff_flow_histo;

    tick_t Mop_fetch_slip;
    tick_t Mop_fetch2decode_slip;
    tick_t Mop_decode_slip;
    tick_t Mop_decode2commit_slip;
    tick_t Mop_commit_slip;

    tick_t uop_decode2alloc_slip;
    tick_t uop_alloc2ready_slip;
    tick_t uop_ready2issue_slip;
    tick_t uop_issue2exec_slip;
    tick_t uop_exec2complete_slip;
    tick_t uop_complete2commit_slip;

    /* oracle stats */
    counter_t oracle_total_insn;
    counter_t oracle_inst_undo;
    counter_t oracle_total_uops;
    counter_t oracle_uop_undo;
    counter_t oracle_total_eff_uops;
    counter_t oracle_eff_uop_undo;
    counter_t oracle_unknown_insn;
    counter_t oracle_total_refs;
    counter_t oracle_total_loads;
    counter_t oracle_total_branches;
    counter_t oracle_total_calls;
    counter_t oracle_num_insn;
    counter_t oracle_num_refs;
    counter_t oracle_num_loads;
    counter_t oracle_num_branches;
    counter_t MopQ_occupancy;
    counter_t MopQ_empty_cycles;
    counter_t MopQ_full_cycles;

    counter_t oracle_resets;

    counter_t feeder_handshakes;
    counter_t handshakes_dropped;
    counter_t handshakes_buffered;
    counter_t handshake_nops_produced;
  } stat;

  /*************/
  /* FUNCTIONS */
  /*************/
  core_t(const int core_id);
  seq_t new_action_id(void);

  struct odep_t * get_odep_link(void);
  void return_odep_link(struct odep_t * const p);

  void reg_common_stats(xiosim::stats::StatsDatabase* sdb);
  void reg_stats(xiosim::stats::StatsDatabase* sdb);

  void update_stopwatch(const Mop_t* Mop);

  protected:

  seq_t global_action_id; /* tag for squashable "actions" */

  struct odep_t * odep_free_pool;     /* for uop->odep links */
  int odep_free_pool_debt;

  std::vector<tick_t> stopwatches;
  std::vector<FILE*> stopwatch_files;
};

#endif /* ZESTO_UARCH_INCLUDED */
