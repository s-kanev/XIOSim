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
 *
 * NOTE: This file (zesto-oracle.c) contains code directly and
 * indirectly derived from previous SimpleScalar source files.
 * These sections are demarkated with "<SIMPLESCALAR>" and
 * "</SIMPLESCALAR>" to specify the start and end, respectively, of
 * such source code.  Such code is bound by the combination of terms
 * and agreements from both Zesto and SimpleScalar.  In case of any
 * conflicting terms (for example, but not limited to, use by
 * commercial entities), the more restrictive terms shall take
 * precedence (e.g., commercial and for-profit entities may not
 * make use of the code without a license from SimpleScalar, LLC).
 * The SimpleScalar terms and agreements are replicated below as per
 * their original requirements.
 *
 * SimpleScalar Ô Tool Suite
 * © 1994-2003 Todd M. Austin, Ph.D. and SimpleScalar, LLC
 * All Rights Reserved.
 * 
 * THIS IS A LEGAL DOCUMENT BY DOWNLOADING SIMPLESCALAR, YOU ARE AGREEING TO
 * THESE TERMS AND CONDITIONS.
 * 
 * No portion of this work may be used by any commercial entity, or for any
 * commercial purpose, without the prior, written permission of SimpleScalar,
 * LLC (info@simplescalar.com). Nonprofit and noncommercial use is permitted as
 * described below.
 * 
 * 1. SimpleScalar is provided AS IS, with no warranty of any kind, express or
 * implied. The user of the program accepts full responsibility for the
 * application of the program and the use of any results.
 * 
 * 2. Nonprofit and noncommercial use is encouraged.  SimpleScalar may be
 * downloaded, compiled, executed, copied, and modified solely for nonprofit,
 * educational, noncommercial research, and noncommercial scholarship purposes
 * provided that this notice in its entirety accompanies all copies. Copies of
 * the modified software can be delivered to persons who use it solely for
 * nonprofit, educational, noncommercial research, and noncommercial
 * scholarship purposes provided that this notice in its entirety accompanies
 * all copies.
 * 
 * 3. ALL COMMERCIAL USE, AND ALL USE BY FOR PROFIT ENTITIES, IS EXPRESSLY
 * PROHIBITED WITHOUT A LICENSE FROM SIMPLESCALAR, LLC (info@simplescalar.com).
 * 
 * 4. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 * 
 * 5. Noncommercial and nonprofit users may distribute copies of SimpleScalar
 * in compiled or executable form as set forth in Section 2, provided that
 * either: (A) it is accompanied by the corresponding machine-readable source
 * code, or (B) it is accompanied by a written offer, with no time limit, to
 * give anyone a machine-readable copy of the corresponding source code in
 * return for reimbursement of the cost of distribution. This written offer
 * must permit verbatim duplication by anyone, or (C) it is distributed by
 * someone who received only the executable form, and is accompanied by a copy
 * of the written offer of source code.
 * 
 * 6. SimpleScalar was developed by Todd M. Austin, Ph.D. The tool suite is
 * currently maintained by SimpleScalar LLC (info@simplescalar.com). US Mail:
 * 2395 Timbercrest Court, Ann Arbor, MI 48105.
 * 
 * Copyright © 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 *
 */

#include "zesto-structs.h"

/* state for the processor microarchitecture (per core) */
class core_t {

  friend class core_oracle_t;

/* (See get_uop_array/return_uop_array functions)
   The following struct and the functions after that are for
   implementing our own "malloc/free" for the uop arrays.  The
   original Mop_t implementation contained a static uop_t array
   sized for the worst-case flow, which caused the Mop_t struct to
   be very large and unwieldly.

   The outside world just wants a struct uop_t *, but
   uop_array_pool has additional fields (namely the size and a
   pointer for chaining all of the free entries together).  This
   unfortunately means that when you return the struct uop_t * to
   the free pool we have to do some ugly pointer munging to get the
   address of the overall uop_array_pool struct.  However, you'll
   be fine if you just use the interface functions, and you
   probably don't even need to do that. */
  struct uop_array_t {
    int size; /* 4 bytes */
    struct uop_array_t * next; /* 4 bytes */
    int padding0; /* 4 bytes */
    int padding1; /* 4 bytes  - want uop array to start aligned to 16 bytes */
    struct uop_t uop[0]; /* we calloc different sized arrays and use this as the
                            base for the array (see the original simplescalar
                            cache data block structure) */
  };

  public:

  /* pointer to configuration information */
  struct core_knobs_t * knobs;

  /********************************/
  /* "ISA" STATE (used by oracle) */
  /********************************/
  struct thread_t * current_thread;
  int id; /* core-id */
  tick_t sim_cycle; /* core-specific cycle counter */
  double cpu_speed; /* current core frequency in MHz */
  double ns_passed; /* used to sync with uncore */

  counter_t num_emergency_recoveries;
  int last_emergency_recovery_count; /* inst count at last recover to detect an unrecoverable situation */

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
    counter_t IQ_uop_occupancy;
    counter_t IQ_eff_uop_occupancy;
    counter_t IQ_full_cycles;
    counter_t IQ_empty_cycles;
    struct stat_stat_t *fetch_stall;

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
    struct stat_stat_t *decode_stall;

    /* alloc stage */
    counter_t alloc_insn;
    counter_t alloc_uops;
    counter_t alloc_eff_uops;
    counter_t regfile_reads;
    counter_t fp_regfile_reads;
    counter_t ROB_writes;
    struct stat_stat_t *alloc_stall;

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

    counter_t eio_commit_insn; /* num instructions consumed in EIO file */

    counter_t commit_deadlock_flushes;

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

    struct stat_stat_t *commit_stall;

    int flow_count;
    int eff_flow_count;
    struct stat_stat_t *flow_histo;
    struct stat_stat_t *eff_flow_histo;

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
    counter_t oracle_num_refs;
    counter_t oracle_num_loads;
    counter_t oracle_num_branches;
    counter_t oracle_bogus_cycles;
    counter_t MopQ_occupancy;
    counter_t MopQ_full_cycles;

    counter_t oracle_resets;
  } stat;


  int num_signals_in_pipe;

  /*************/
  /* FUNCTIONS */
  /*************/
  core_t(const int core_id);
  seq_t new_action_id(void);

  struct uop_t * get_uop_array(const int size);
  void return_uop_array(struct uop_t * const p);

  struct odep_t * get_odep_link(void);
  void return_odep_link(struct odep_t * const p);

  static void zero_uop(struct uop_t * const uop);
  static void zero_Mop(struct Mop_t * const Mop);

  protected:

  seq_t global_action_id; /* tag for squashable "actions" */

  bool static_members_initialized;
  static seq_t global_seq; /* This is shared among all cores */
  /* to reduce overhead of constantly malloc/freeing arrays to
     store the uop flows of all of the Mops, we just maintain our
     own free pool.  pool entry i contains arrays with length i. */
  struct uop_array_t * uop_array_pool[MD_MAX_FLOWLEN+2+1]; /* the extra +2+1 is for REP u-jump uops */
  struct odep_t * odep_free_pool;     /* for uop->odep links */
  int odep_free_pool_debt;

  void uop_init(struct uop_t * const uop);
};

#endif /* ZESTO_UARCH_INCLUDED */
