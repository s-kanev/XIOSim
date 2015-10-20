#ifndef ZESTO_STRUCTS_INCLUDED
#define ZESTO_STRUCTS_INCLUDED

/* zesto-structs.h - Zesto basic structure definitions
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
 */

extern "C" {
#include "xed-interface.h"
}
#include "fu.h"
#include "regs.h"
#include "uop_cracker.h"
#include "zesto-core.h"

#define MAX_IDEPS 3
#define MAX_ODEPS 2

#define MAX_HYBRID_BPRED 8 /* maximum number of sub-components in a hybrid branch predictor */
#define MAX_DECODE_WIDTH 16
#define MAX_EXEC_WIDTH 16
#define MAX_PREFETCHERS 4 /* per cache; so IL1 can have MAX_PREFETCHERS prefetchers independent of the DL1 or LLC */

/* structure for a uop's list of output dependencies (dataflow children) */
struct odep_t {
  struct uop_t * uop;
  int op_num;
  bool aflags; /* TRUE if this odep link is for an AFLAGS dependency */
  struct odep_t * next;
};

using namespace xiosim;
using namespace xiosim::x86;

/* uArch micro-op structure */
struct alignas(16) uop_t
{
  struct core_t * core; /* back pointer to core so we know which core this uop is from */
  struct Mop_t * Mop; /* back pointer to parent marco-inst */

  struct {
    unsigned int opflags; /* decoded flags */

    bool has_imm; /* TRUE if this uop has an immediate (which is stored in the next two consecutive uops */
    bool is_imm; /* TRUE if this uop is not a real uop, but just part of an immediate */
    xed_reg_enum_t idep_name[MAX_IDEPS]; /* register input dependencies */
    xed_reg_enum_t odep_name[MAX_ODEPS]; /* register output dependencies */

    int mem_size; /* size of memory data; if load/store */

    bool BOM; /* TRUE if first uop of macro (Beginning of Macro) */
    bool EOM; /* TRUE if last uop of macro (End of Macro) */

    /* decode flags (is_load, etc.) */
    bool is_ctrl; /* Is branch/jump/call etc.? */
    bool is_load; /* Is load? */
    bool is_sta;  /* Is store-address uop? */
    bool is_std;  /* Is store-data uop? */
    bool is_nop;  /* Is NOP? */
    bool is_fence; /* Is fence? */
    bool is_light_fence; /* Light fence? (heavy vs light == wait for commit vs wait for WB) */
    bool is_agen; /* Is AGEN uop (LEA instruction) */
    bool is_fpop; /* Is floating-point op? */

    /* assume unique uop ID assigned when Mop cracked */
    seq_t Mop_seq;
    seq_t uop_seq; /* we use a seq that's a combination of the Mop seq and the uop flow-index */

    enum fu_class FU_class; /* What type of ALU does this uop use? */

    /* uop-fusion */
    bool in_fusion;      /* this uop belongs to a fusion of two or more uops */
    bool is_fusion_head; /* first uop of a fused set? */
    int fusion_size;    /* total number of uops in this fused set */
    struct uop_t * fusion_head; /* pointer to first uop of a fused set */
    struct uop_t * fusion_next; /* pointer to next uop in this fused set */
  } decode;

  struct {
    int ROB_index;
    int RS_index;
    int LDQ_index;
    int STQ_index;
    int port_assignment; /* execution port binding */
    bool full_fusion_allocated; /* TRUE when all sub-uops of a fused-uop have been successfully alloc'd */
  } alloc;

  struct {
    bool in_readyQ;
    seq_t action_id;
    /* these pointers are only between uops in the OOO core (RS,ROB,etc.) */
    struct uop_t * idep_uop[MAX_IDEPS];
    struct odep_t * odep_uop;

    bool ivalue_valid[MAX_IDEPS];   /* the value is valid */
    bool ovalue_valid;

    /* for load instructions */
    tick_t when_data_loaded;
    tick_t when_addr_translated;
    
    int uops_in_RS; /* used only for fusion-head */
    int num_replays; /* number of times reached exec but ivalue not ready */
  } exec;

  struct {
    /* memory information */
    int mem_op_index; /* which memory operand of the Mop is this */
    md_addr_t virt_addr;
    md_paddr_t phys_addr;

    /* register dependence pointers */
    struct uop_t * idep_uop[MAX_IDEPS];
    struct odep_t * odep_uop;

    // XXX: Clean-up
    bool is_repeated; /* uses cache hierarchy or mem-repater */
    bool is_sync_op;  /* repeater-bound ops -- are they wait/signal */
    bool recover_inst; /* TRUE if next inst is at wrong PC */
  } oracle;

  struct {
    tick_t when_decoded;
    tick_t when_allocated;
    tick_t when_itag_ready[MAX_IDEPS]; /* these track readiness of dependencies, which is different from the */
    tick_t when_otag_ready;            /* readiness of the actual VALUE since schedule and exec are decoupled. */
    tick_t when_ival_ready[MAX_IDEPS]; /* when_oval_ready == when_completed */
    tick_t when_ready;
    tick_t when_issued;
    tick_t when_exec;
    tick_t when_completed;
  } timing;

  int flow_index;

  /* all sizes/loop lengths known at compile time; compiler
     should be able to optimize this pretty well.  Assumes
     uop is aligned to 16 bytes. */
  void zero() {
#if USE_SSE_MOVE
    char * addr = (char*) this;
    assert((long long) addr % 16 == 0);
    int bytes = sizeof(*this);
    int remainder = bytes - (bytes>>7)*128;

    /* zero xmm0 */
    asm ("xorps %%xmm0, %%xmm0"
         : : : "%xmm0");
    /* clear the uop 128 bytes at a time */
    for(int i=0;i<bytes>>7;i++)
    {
      asm ("movaps %%xmm0,    (%0)\n\t"
           "movaps %%xmm0,  16(%0)\n\t"
           "movaps %%xmm0,  32(%0)\n\t"
           "movaps %%xmm0,  48(%0)\n\t"
           "movaps %%xmm0,  64(%0)\n\t"
           "movaps %%xmm0,  80(%0)\n\t"
           "movaps %%xmm0,  96(%0)\n\t"
           "movaps %%xmm0, 112(%0)\n\t"
           : : "r"(addr) : "memory");
      addr += 128;
    }

    /* handle any remaining bytes; optimizer should remove this
       when sizeof(uop) has no remainder */
    for(int i=0;i<remainder>>3;i++)
    {
      asm ("movlps %%xmm0,   (%0)\n\t"
           : : "r"(addr) : "memory");
      addr += 8;
    }
#else
    memset(this,0,sizeof(*this));
#endif
  }

  uop_t() {
      zero();
      memset(&this->alloc,-1,sizeof(this->alloc));
      this->oracle.mem_op_index = -1;
      this->decode.Mop_seq = (seq_t)-1;
      this->decode.uop_seq = (seq_t)-1;
      this->alloc.port_assignment = -1;

      this->timing.when_decoded = TICK_T_MAX;
      this->timing.when_allocated = TICK_T_MAX;
      for(int i = 0; i < MAX_IDEPS; i++) {
          this->timing.when_itag_ready[i] = TICK_T_MAX;
          this->timing.when_ival_ready[i] = TICK_T_MAX;
      }
      this->timing.when_otag_ready = TICK_T_MAX;
      this->timing.when_ready = TICK_T_MAX;
      this->timing.when_issued = TICK_T_MAX;
      this->timing.when_exec = TICK_T_MAX;
      this->timing.when_completed = TICK_T_MAX;

      this->exec.action_id = (seq_t)-1;
      this->exec.when_data_loaded = TICK_T_MAX;
      this->exec.when_addr_translated = TICK_T_MAX;
  }

};

/* x86 Macro-op structure */
struct alignas(16) Mop_t
{
  struct core_t * core; /* back pointer to core so we know which core this uop is from */
  int valid;

  struct {
    md_addr_t PC;
    md_addr_t pred_NPC;
    md_addr_t ftPC;
    md_inst_t inst;
    bool first_byte_requested;
    bool last_byte_requested;

    seq_t jeclear_action_id;
    class bpred_state_cache_t * bpred_update; /* bpred update info */
  } fetch;

  struct {
    xed_decoded_inst_t inst;
    inst_flags_t opflags;
    size_t flow_length;
    size_t last_uop_index; /* index of last uop (maybe != flow_length due to imm's) */
    md_addr_t targetPC; /* for branches, target PC */

    bool is_trap;
    bool is_ctrl;

    size_t last_stage_index; /* index of next uop to remove from decode pipe */
  } decode;

  /* pointer to the raw uops that implement this Mop */
  struct uop_t * uop;

  struct {
    int complete_index; /* points to first uop that has not yet completed execution (-1 == all done) */
    int commit_index; /* points to first uop that has not yet been committed */
    bool jeclear_in_flight;
  } commit;

  struct {
    md_addr_t NextPC;
    seq_t seq;
    bool zero_rep; /* TRUE if inst has REP of zero */
    bool spec_mode; /* this instruction is on wrong path? */
    /* XXX: In most cases this can be inferred from NextPC != fetch.PC + inst.len,
     * except when we encountered and unknown or overwritten instruction from
     * the feeder. So we keep oracle info about branches from feeder. */
    bool taken_branch;
    bool recover_inst; /* TRUE if the NPC for this Mop is wrong */
  } oracle;

  struct {
    tick_t when_fetch_started;
    tick_t when_fetched;
    tick_t when_MS_started; /* for complex Mops needing the MS, cycle on which MS starts delivering uops */
    tick_t when_decode_started;
    tick_t when_decode_finished;
    tick_t when_commit_started;
    tick_t when_commit_finished;
  } timing;

  struct {
    int num_uops;
    int num_eff_uops;
    int num_refs;
    int num_loads;
    int num_branches;
  } stat;

  void zero() {
#if USE_SSE_MOVE
      char * addr = (char*) this;
      assert((long long) addr % 16 == 0);
      int bytes = sizeof(*this);
      int remainder = bytes - (bytes>>6)*64;

      /* zero xmm0 */
      asm ("xorps %%xmm0, %%xmm0"
           : : : "%xmm0");
      /* clear the uop 64 bytes at a time */
      for(int i = 0; i < bytes >> 6; i++) {
          asm ("movaps %%xmm0,   (%0)\n\t"
               "movaps %%xmm0, 16(%0)\n\t"
               "movaps %%xmm0, 32(%0)\n\t"
               "movaps %%xmm0, 48(%0)\n\t"
               : : "r"(addr) : "memory");
          addr += 64;
      }

      /* handle any remaining bytes */
      for (int i = 0; i < remainder >> 3; i++) {
          asm ("movlps %%xmm0,   (%0)\n\t"
               : : "r"(addr) : "memory");
          addr += 8;
      }
#else
      memset(this, 0, sizeof(*this));
#endif
  }

  void clear() {
      this->zero();
      this->timing.when_fetch_started = TICK_T_MAX;
      this->timing.when_fetched = TICK_T_MAX;
      this->timing.when_MS_started = TICK_T_MAX;
      this->timing.when_decode_started = TICK_T_MAX;
      this->timing.when_decode_finished = TICK_T_MAX;
      this->timing.when_commit_started = TICK_T_MAX;
      this->timing.when_commit_finished = TICK_T_MAX;
      this->valid = true;
  }

  void allocate_uops(void) {
      uop = x86::get_uop_array(decode.flow_length);
      for (size_t i = 0; i < decode.flow_length; i++) {
          uop[i].flow_index = i;
          uop[i].Mop = this;
      }
  }

  void clear_uops(void) {
      x86::return_uop_array(uop, decode.flow_length);

      uop = nullptr;
  }
};


/* holds all of the parameters for a core, plus any additional helper variables
   for command line parsing (e.g., config strings) */
struct core_knobs_t
{
  const char * model;
  double default_cpu_speed; /* in MHz */
  const char * dvfs_opt_str;
  int dvfs_interval;

  struct {
    int byteQ_size;
    int byteQ_linesize; /* in bytes */
    int depth; /* predecode pipe */
    int width; /* predecode pipe */
    int IQ_size;
    int jeclear_delay;

    int   num_bpred_components;
    const char *bpred_opt_str[MAX_HYBRID_BPRED];
    const char *fusion_opt_str;
    const char *dirjmpbtb_opt_str;
    const char *indirjmpbtb_opt_str;
    const char *ras_opt_str;
  } fetch;

  struct {
    int depth; /* decode pipe */
    int width; /* decode pipe */
    int target_stage;        /* stage in which a wrong taken BTB target is detected and corrected */
    int *max_uops; /* maximum number of uops emittable per decoder */
    int MS_latency; /* number of cycles from decoder[0] to uROM/MS */
    int uopQ_size;
    int fusion_mode; /* bitmask of which fusion types are allowed */
    int decoders[MAX_DECODE_WIDTH];
    int num_decoder_specs;
    int branch_decode_limit; /* maximum number of branches decoded per cycle */
    bool fusion_none; /* this takes precedence over -fusion:all */
    bool fusion_all;  /* and then this takes precedence over the subsequent flags */
    bool fusion_load_op;
    bool fusion_fp_load_op;
    bool fusion_sta_std;
    bool fusion_partial;
    bool fusion_load_op_st;
  } decode;

  struct {
    int depth; /* alloc pipe */
    int width; /* alloc pipe */
    bool drain_flush; /* use drain flush? */
  } alloc;

  struct {
    int RS_size;
    int LDQ_size;
    int STQ_size;
    struct {
      int num_FUs;
      int *ports;
    } port_binding[NUM_FU_CLASSES];
    int num_exec_ports;
    int payload_depth;
    int fu_bindings[NUM_FU_CLASSES][MAX_EXEC_WIDTH];
    int num_bindings[NUM_FU_CLASSES];
    int latency[NUM_FU_CLASSES];
    int issue_rate[NUM_FU_CLASSES];
    const char * memdep_opt_str;
    int fp_penalty; /* extra cycles to forward to FP cluster */
    bool tornado_breaker;
    bool throttle_partial;
    const char * repeater_opt_str;
  } exec;

  struct {
    /* prefetch arguments */
    int IL1_num_PF; int IL1_PFFsize; int IL1_PFthresh; int IL1_PFmax; int IL1_PF_buffer_size;
    int IL1_PF_filter_size; int IL1_PF_filter_reset; bool IL1_PF_on_miss;
    int IL1_WMinterval; double IL1_low_watermark; double IL1_high_watermark;
    float IL1_magic_hit_rate;
    int DL1_num_PF; int DL1_PFFsize; int DL1_PFthresh; int DL1_PFmax; int DL1_PF_buffer_size;
    int DL1_PF_filter_size; int DL1_PF_filter_reset; bool DL1_PF_on_miss;
    int DL1_WMinterval; double DL1_low_watermark; double DL1_high_watermark;
    const char * DL1_MSHR_cmd;
    float DL1_magic_hit_rate;
    int DL2_num_PF; int DL2_PFFsize; int DL2_PFthresh; int DL2_PFmax; int DL2_PF_buffer_size;
    int DL2_PF_filter_size; int DL2_PF_filter_reset; bool DL2_PF_on_miss;
    int DL2_WMinterval; double DL2_low_watermark; double DL2_high_watermark;
    const char * DL2_MSHR_cmd;
    float DL2_magic_hit_rate;

    /* for storing command line parameters */
    const char * IL1_opt_str;
    const char * ITLB_opt_str;
    const char * DL1_opt_str;
    const char * DL2_opt_str;
    const char * DTLB_opt_str;
    const char * DTLB2_opt_str;

    const char * IL1PF_opt_str[MAX_PREFETCHERS];
    const char * DL1PF_opt_str[MAX_PREFETCHERS];
    const char * DL2PF_opt_str[MAX_PREFETCHERS];

    const char * IL1_controller_opt_str;
    const char * ITLB_controller_opt_str;
    const char * DL1_controller_opt_str;
    const char * DL2_controller_opt_str;
    const char * DTLB_controller_opt_str;
    const char * DTLB2_controller_opt_str;

    bool DL1_rep_req;
  } memory;

  struct {
    int ROB_size;
    int width;
    int branch_limit; /* maximum number of branches committed per cycle */
    int pre_commit_depth;
  } commit;

  struct {
    bool compute;
    int rtp_interval;
    const char * rtp_filename;
  } power;

  int scheduler_tick;

  /* A command line flag specifying the core allocation policy. Valid options:
   * "gang", "local", or "penalty". See pintool/base_allocator.h for more
   * details.
   */
  const char * allocator;

  /* A command line flag specifying the speedup model to use when calculating
   * core allocations. Valid options: "linear" or "log". See
   * pintool/base_speedup_model.h for more details.
   */
  const char * speedup_model;

  /* A command line flag specifying the optimization target when computing core
   * allocations. Valid options: "energy" or "throughput". See
   * pintool/base_speedup_model.h for more details.
   */
  const char * allocator_opt_target;
};

extern struct core_t ** cores;
extern struct core_knobs_t knobs;

#endif /* ZESTO_STRUCTS_INCLUDED */
