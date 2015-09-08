/* zesto-oracle.cpp - Zesto oracle functional simulator class
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
 * NOTE: This file (zesto-oracle.cpp) contains code directly and
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
 */
#define ZESTO_ORACLE_C

#include <stddef.h>
#include "host.h"
#include "misc.h"
#include "thread.h"
#include "memory.h"
#include "decode.h"
#include "uop_cracker.h"

#include "zesto-core.h"
#include "zesto-oracle.h"
#include "zesto-fetch.h"
#include "zesto-bpred.h"
#include "zesto-decode.h"
#include "zesto-alloc.h"
#include "zesto-exec.h"
#include "zesto-commit.h"

#include <stack>

using namespace std;

/* CONSTRUCTOR */
core_oracle_t::core_oracle_t(struct core_t* const arg_core)
    : spec_mode(false)
    , hosed(false)
    , Mop_seq(0)
    , MopQ(NULL)
    , MopQ_head(0)
    , MopQ_tail(0)
    , MopQ_num(0)
    , current_Mop(NULL)
    , MopQ_spec_num(0) {
    core = arg_core;
    struct core_knobs_t* knobs = core->knobs;

    /* MopQ should be large enough to support all in-flight
       instructions.  We assume one entry per "slot" in the machine
       (even though in uop-based structures, multiple slots may
       correspond to a single Mop), and then add a few and round up
       just in case. */
    int temp_MopQ_size = knobs->commit.ROB_size + knobs->alloc.depth * knobs->alloc.width +
                         knobs->decode.uopQ_size + knobs->decode.depth * knobs->decode.width +
                         knobs->fetch.IQ_size + knobs->fetch.depth * knobs->fetch.width +
                         knobs->fetch.byteQ_size + 64;

    MopQ_size = 1 << ((int)ceil(log(temp_MopQ_size) / log(2.0)));

    int res = posix_memalign((void**)&MopQ, 16, MopQ_size * sizeof(*MopQ));
    if (!MopQ || res != 0)
        fatal("failed to calloc MopQ");
    assert(sizeof(*MopQ) % 16 == 0);         // size of Mop is mult of 16
    assert(sizeof(struct uop_t) % 16 == 0);  // size of uop is mult of 16

    for (int i = 0; i < MopQ_size; i++) {
        MopQ[i].core = core;
        MopQ[i].clear();
        MopQ[i].uop = NULL;
    }

    shadow_MopQ = new Buffer<handshake_container_t>(MopQ_size);
}

/* register oracle-related stats in the stat-database (sdb) */
void core_oracle_t::reg_stats(struct stat_sdb_t* const sdb) {
    char buf[1024];
    char buf2[1024];
    struct thread_t* arch = core->current_thread;

    stat_reg_note(sdb, "\n#### ORACLE STATS ####");
    sprintf(buf, "c%d.oracle_num_insn", arch->id);
    sprintf(buf2, "c%d.oracle_total_insn - c%d.oracle_insn_undo", arch->id, arch->id);
    stat_reg_formula(sdb, true, buf, "number of instructions executed by oracle", buf2, "%12.0f");
    sprintf(buf, "c%d.oracle_total_insn", arch->id);
    stat_reg_counter(sdb,
                     true,
                     buf,
                     "total number of instructions executed by oracle, including misspec",
                     &core->stat.oracle_total_insn,
                     0,
                     TRUE,
                     NULL);
    sprintf(buf, "c%d.oracle_insn_undo", arch->id);
    stat_reg_counter(sdb,
                     false,
                     buf,
                     "total number of instructions undone by oracle (misspeculated insts)",
                     &core->stat.oracle_inst_undo,
                     0,
                     TRUE,
                     NULL);
    sprintf(buf, "c%d.oracle_unknown_insn", arch->id);
    stat_reg_counter(sdb,
                     true,
                     buf,
                     "total number of unsupported instructions turned into NOPs by oracle",
                     &core->stat.oracle_unknown_insn,
                     0,
                     TRUE,
                     NULL);
    sprintf(buf, "c%d.oracle_num_uops", arch->id);
    sprintf(buf2, "c%d.oracle_total_uops - c%d.oracle_uop_undo", arch->id, arch->id);
    stat_reg_formula(sdb, true, buf, "number of uops executed by oracle", buf2, "%12.0f");
    sprintf(buf, "c%d.oracle_total_uops", arch->id);
    stat_reg_counter(sdb,
                     true,
                     buf,
                     "total number of uops executed by oracle, including misspec",
                     &core->stat.oracle_total_uops,
                     0,
                     TRUE,
                     NULL);
    sprintf(buf, "c%d.oracle_uop_undo", arch->id);
    stat_reg_counter(sdb,
                     false,
                     buf,
                     "total number of uops undone by oracle",
                     &core->stat.oracle_uop_undo,
                     0,
                     TRUE,
                     NULL);
    sprintf(buf, "c%d.oracle_num_eff_uops", arch->id);
    sprintf(buf2, "c%d.oracle_total_eff_uops - c%d.oracle_eff_uop_undo", arch->id, arch->id);
    stat_reg_formula(sdb, true, buf, "number of effective uops executed by oracle", buf2, "%12.0f");
    sprintf(buf, "c%d.oracle_total_eff_uops", arch->id);
    stat_reg_counter(sdb,
                     true,
                     buf,
                     "total number of effective uops executed by oracle, including misspec",
                     &core->stat.oracle_total_eff_uops,
                     0,
                     TRUE,
                     NULL);
    sprintf(buf, "c%d.oracle_eff_uop_undo", arch->id);
    stat_reg_counter(sdb,
                     false,
                     buf,
                     "total number of effective uops undone by oracle",
                     &core->stat.oracle_eff_uop_undo,
                     0,
                     TRUE,
                     NULL);

    sprintf(buf, "c%d.oracle_IPC", arch->id);
    sprintf(buf2, "c%d.oracle_num_insn / c%d.sim_cycle", arch->id, arch->id);
    stat_reg_formula(sdb, true, buf, "IPC at oracle", buf2, NULL);
    sprintf(buf, "c%d.oracle_uPC", arch->id);
    sprintf(buf2, "c%d.oracle_num_uops / c%d.sim_cycle", arch->id, arch->id);
    stat_reg_formula(sdb, true, buf, "uPC at oracle", buf2, NULL);
    sprintf(buf, "c%d.oracle_euPC", arch->id);
    sprintf(buf2, "c%d.oracle_num_eff_uops / c%d.sim_cycle", arch->id, arch->id);
    stat_reg_formula(sdb, true, buf, "euPC at oracle", buf2, NULL);
    sprintf(buf, "c%d.oracle_total_IPC", arch->id);
    sprintf(buf2, "c%d.oracle_total_insn / c%d.sim_cycle", arch->id, arch->id);
    stat_reg_formula(sdb, true, buf, "IPC at oracle, including wrong-path", buf2, NULL);
    sprintf(buf, "c%d.oracle_total_uPC", arch->id);
    sprintf(buf2, "c%d.oracle_total_uops / c%d.sim_cycle", arch->id, arch->id);
    stat_reg_formula(sdb, true, buf, "uPC at oracle, including wrong-path", buf2, NULL);
    sprintf(buf, "c%d.oracle_total_euPC", arch->id);
    sprintf(buf2, "c%d.oracle_total_eff_uops / c%d.sim_cycle", arch->id, arch->id);
    stat_reg_formula(sdb, true, buf, "euPC at oracle, including wrong-path", buf2, NULL);
    sprintf(buf, "c%d.avg_oracle_flowlen", arch->id);
    sprintf(buf2, "c%d.oracle_num_uops / c%d.oracle_num_insn", arch->id, arch->id);
    stat_reg_formula(sdb, true, buf, "uops per instruction at oracle", buf2, NULL);
    sprintf(buf, "c%d.avg_oracle_eff_flowlen", arch->id);
    sprintf(buf2, "c%d.oracle_num_eff_uops / c%d.oracle_num_insn", arch->id, arch->id);
    stat_reg_formula(sdb, true, buf, "effective uops per instruction at oracle", buf2, NULL);
    sprintf(buf, "c%d.avg_oracle_total_flowlen", arch->id);
    sprintf(buf2, "c%d.oracle_total_uops / c%d.oracle_total_insn", arch->id, arch->id);
    stat_reg_formula(
        sdb, true, buf, "uops per instruction at oracle, including wrong-path", buf2, NULL);
    sprintf(buf, "c%d.avg_oracle_total_eff_flowlen", arch->id);
    sprintf(buf2, "c%d.oracle_total_eff_uops / c%d.oracle_total_insn", arch->id, arch->id);
    stat_reg_formula(sdb,
                     true,
                     buf,
                     "effective uops per instruction at oracle, including wrong-path",
                     buf2,
                     NULL);

    sprintf(buf, "c%d.oracle_num_refs", arch->id);
    stat_reg_counter(sdb,
                     true,
                     buf,
                     "total number of loads and stores executed by oracle",
                     &core->stat.oracle_num_refs,
                     0,
                     TRUE,
                     NULL);
    sprintf(buf, "c%d.oracle_num_loads", arch->id);
    stat_reg_counter(sdb,
                     true,
                     buf,
                     "total number of loads executed by oracle",
                     &core->stat.oracle_num_loads,
                     0,
                     TRUE,
                     NULL);
    sprintf(buf2, "c%d.oracle_num_refs - c%d.oracle_num_loads", arch->id, arch->id);
    sprintf(buf, "c%d.oracle_num_stores", arch->id);
    stat_reg_formula(sdb, true, buf, "total number of stores executed by oracle", buf2, "%12.0f");
    sprintf(buf, "c%d.oracle_num_branches", arch->id);
    stat_reg_counter(sdb,
                     true,
                     buf,
                     "total number of branches executed by oracle",
                     &core->stat.oracle_num_branches,
                     0,
                     TRUE,
                     NULL);

    sprintf(buf, "c%d.oracle_total_refs", arch->id);
    stat_reg_counter(sdb,
                     true,
                     buf,
                     "total number of loads and stores executed by oracle, including wrong-path",
                     &core->stat.oracle_total_refs,
                     0,
                     TRUE,
                     NULL);
    sprintf(buf, "c%d.oracle_total_loads", arch->id);
    stat_reg_counter(sdb,
                     true,
                     buf,
                     "total number of loads executed by oracle, including wrong-path",
                     &core->stat.oracle_total_loads,
                     0,
                     TRUE,
                     NULL);
    sprintf(buf2, "c%d.oracle_total_refs - c%d.oracle_total_loads", arch->id, arch->id);
    sprintf(buf, "c%d.oracle_total_stores", arch->id);
    stat_reg_formula(sdb,
                     true,
                     buf,
                     "total number of stores executed by oracle, including wrong-path",
                     buf2,
                     "%12.0f");
    sprintf(buf, "c%d.oracle_total_branches", arch->id);
    stat_reg_counter(sdb,
                     true,
                     buf,
                     "total number of branches executed by oracle, including wrong-path",
                     &core->stat.oracle_total_branches,
                     0,
                     TRUE,
                     NULL);
    sprintf(buf, "c%d.oracle_total_calls", arch->id);
    stat_reg_counter(sdb,
                     true,
                     buf,
                     "total number of function calls executed by oracle, including wrong-path",
                     &core->stat.oracle_total_calls,
                     0,
                     TRUE,
                     NULL);
    sprintf(buf, "c%d.MopQ_occupancy", arch->id);
    stat_reg_counter(
        sdb, true, buf, "total oracle MopQ occupancy", &core->stat.MopQ_occupancy, 0, TRUE, NULL);
    sprintf(buf, "c%d.MopQ_avg", arch->id);
    sprintf(buf2, "c%d.MopQ_occupancy/c%d.sim_cycle", arch->id, arch->id);
    stat_reg_formula(sdb, true, buf, "average oracle MopQ occupancy", buf2, NULL);
    sprintf(buf, "c%d.MopQ_full", arch->id);
    stat_reg_counter(sdb,
                     true,
                     buf,
                     "total cycles oracle MopQ was full",
                     &core->stat.MopQ_full_cycles,
                     0,
                     TRUE,
                     NULL);
    sprintf(buf, "c%d.MopQ_frac_full", arch->id);
    sprintf(buf2, "c%d.MopQ_full/c%d.sim_cycle", arch->id, arch->id);
    stat_reg_formula(sdb, true, buf, "fraction of cycles oracle MopQ was full", buf2, NULL);
    sprintf(buf, "c%d.oracle_bogus_cycles", arch->id);
    stat_reg_counter(sdb,
                     true,
                     buf,
                     "total cycles oracle stalled on invalid wrong-path insts",
                     &core->stat.oracle_bogus_cycles,
                     0,
                     TRUE,
                     NULL);
    sprintf(buf, "c%d.oracle_frac_bogus", arch->id);
    sprintf(buf2, "c%d.oracle_bogus_cycles/c%d.sim_cycle", arch->id, arch->id);
    stat_reg_formula(sdb,
                     true,
                     buf,
                     "fraction of cycles oracle stalled on invalid wrong-path insts",
                     buf2,
                     NULL);
    sprintf(buf, "c%d.oracle_emergency_recoveries", arch->id);
    stat_reg_counter(sdb,
                     true,
                     buf,
                     "number of times this thread underwent an emergency recovery",
                     &core->num_emergency_recoveries,
                     0,
                     FALSE,
                     NULL);
}

void core_oracle_t::update_occupancy(void) {
    /* MopQ */
    core->stat.MopQ_occupancy += MopQ_num;
    if (MopQ_num >= MopQ_size)
        core->stat.MopQ_full_cycles++;
}

struct Mop_t* core_oracle_t::get_Mop(const int index) {
    return &MopQ[index];
}

struct Mop_t* core_oracle_t::get_oldest_Mop() {
    if (MopQ_num == 0)
        return NULL;

    return &MopQ[MopQ_head];
}

int core_oracle_t::get_index(const struct Mop_t* const Mop) { return Mop - MopQ; }

int core_oracle_t::next_index(const int index) {
    return modinc(index, MopQ_size);  // return (index+1)%MopQ_size;
}

/* The following code is derived from the original execution
   code in the SimpleScalar/x86 pre-release, but it has been
   extensively modified for the way we're doing things in Zesto.
   Major changes include the handling of REP instructions and
   handling of execution down wrong control paths. */
struct Mop_t* core_oracle_t::exec(const md_addr_t requested_PC) {
    struct thread_t* thread = core->current_thread;
    struct core_knobs_t * knobs = core->knobs;
    size_t flow_index = 0;
    struct Mop_t* Mop = NULL;
    bool* bogus = &core->fetch->bogus;

    if (*bogus) /* are we on a wrong path and fetched bogus insts? */
    {
        assert(spec_mode);
        ZESTO_STAT(core->stat.oracle_bogus_cycles++;)
        return NULL;
    }

    if (current_Mop) /* we already have a Mop */
    {
        /* make sure pipeline has drained */
        if (current_Mop->decode.is_trap) {
            if (MopQ_num > 1) { /* 1 since the trap itself is in the MopQ */
                core->current_thread->consumed = false;
                return nullptr;
            }
        }
        assert(current_Mop->uop->timing.when_ready == TICK_T_MAX);
        assert(current_Mop->uop->timing.when_issued == TICK_T_MAX);
        assert(current_Mop->uop->timing.when_exec == TICK_T_MAX);
        assert(current_Mop->uop->timing.when_completed == TICK_T_MAX);
        return current_Mop;
    } else {
        Mop = &MopQ[MopQ_tail];
    }

    if (MopQ_num >= MopQ_size - 1) {
        core->current_thread->consumed = false;
        return nullptr;
    }

    /* reset Mop state */
    Mop->clear();
    Mop->core = core;

    ZTRACE_PRINT(core->id, "reqPC: %x, feeder_NPC: %x\n", requested_PC, core->fetch->feeder_NPC);
    /* go to next instruction */
    if (requested_PC != core->fetch->feeder_NPC)
        spec_mode = true;

    Mop->oracle.spec_mode = spec_mode;

    /* get the next instruction to execute */
    handshake_container_t* handshake = get_shadow_Mop(Mop);
    zesto_assert(spec_mode == handshake->flags.speculative, NULL);
    /* read encoding supplied by feeder */
    memcpy(&Mop->fetch.inst.code, handshake->ins, xiosim::x86::MAX_ILEN);

//    XXX: We need something along these lines for nukes
//    if (num_Mops_before_feeder() > 0)
//        grab_feeder_state(handshake, false, true);

    // zesto_assert(MopQ_num == shadow_MopQ->size(), NULL);

    /* then decode the instruction */
    x86::decode(Mop);

#if 0
  /* Skip invalid instruction */
  if(Mop->decode.op == OP_NA) {
      core->fetch->invalid = true;
      Mop->decode.op = NOP;
  }
#endif

    x86::decode_flags(Mop);

    core->fetch->feeder_NPC = handshake->flags.brtaken ? handshake->tpc : handshake->npc;

    Mop->fetch.PC = requested_PC;
    Mop->fetch.ftPC = handshake->npc;
    Mop->oracle.taken_branch = handshake->flags.brtaken;
    Mop->oracle.NextPC = handshake->flags.brtaken ? handshake->tpc : handshake->npc;

    if (Mop->decode.is_ctrl)
        Mop->decode.targetPC = handshake->tpc;

    /* set unique id */
    Mop->oracle.seq = Mop_seq++;

    /* Crack Mop into uops */
    x86::crack(Mop);

/* XXX: Handle uop flags. */

/* XXX: Handle uop fusion. */

#if 0
    if(!*bogus)
    {
      int imm_uops_left = 0;
      struct uop_t * fusion_head = NULL; /* if currently fused, points at head. */
      struct uop_t * prev_uop = NULL;

      for(int i=0;i<Mop->decode.flow_length;i++)
      {
        struct uop_t * uop = &Mop->uop[i];
        uop->decode.raw_op = flowtab[i];
        if(!imm_uops_left)
        {
          uop->decode.has_imm = UHASIMM;
          MD_SET_UOPCODE(uop->decode.op,&uop->decode.raw_op);
          uop->decode.opflags = MD_OP_FLAGS(uop->decode.op);

          if(knobs->decode.fusion_mode & FUSION_TYPE(uop))
          {
            assert(prev_uop);
            if(fusion_head == NULL)
            {
              assert(!prev_uop->decode.in_fusion);
              fusion_head = prev_uop;
              fusion_head->decode.in_fusion = true;
              fusion_head->decode.is_fusion_head = true;
              fusion_head->decode.fusion_size = 1;
              fusion_head->decode.fusion_head = fusion_head; /* point at yourself as well */
            }

            uop->decode.in_fusion = true;
            uop->decode.fusion_head = fusion_head;
            fusion_head->decode.fusion_size++;

            prev_uop->decode.fusion_next = uop;
          }
          else
            fusion_head = NULL;

          prev_uop = uop;
        }
        else
        {
          imm_uops_left--; /* don't try to decode the immediates! */
          uop->decode.is_imm = true;
        }

        uop->Mop = Mop; /* back-pointer to parent macro-op */
        if(uop->decode.has_imm)
          imm_uops_left = 2;
      }
    }
    else
    {
      /* If at any point we decode something strange, if we're on the wrong path, we'll just
         abort the instruction.  This will basically bring fetch to a halt until the machine
         gets back on the correct control-flow path. */
      if(spec_mode)
      {
        *bogus = true;
        return NULL;
      }
      else
        fatal("could not locate UCODE flow");
    }
#endif

    for (size_t i = 0; i < Mop->decode.flow_length; i++) {
        Mop->uop[i].core = core;
        Mop->uop[i].flow_index = i;
        Mop->uop[i].Mop = Mop;
        Mop->uop[i].decode.Mop_seq = Mop->oracle.seq;
        Mop->uop[i].decode.uop_seq = (Mop->oracle.seq << x86::UOP_SEQ_SHIFT) + i;
    }

    //XXX: We need proper handling of fusion. For now, keep a minimum STA-STD to make the Atom model happy.
    for (size_t i = 0; i < Mop->decode.flow_length; i++) {
        struct uop_t* uop = &Mop->uop[i];
        if ((knobs->decode.fusion_mode & FUSION_STA_STD) &&
            uop->decode.is_sta) {
            zesto_assert(i < Mop->decode.flow_length - 1, nullptr);
            struct uop_t* next_uop = &Mop->uop[i+1];
            zesto_assert(next_uop->decode.is_std, nullptr);

            uop->decode.in_fusion = true;
            uop->decode.is_fusion_head = true;
            uop->decode.fusion_size = 2;
            uop->decode.fusion_head = uop;
            uop->decode.fusion_next = next_uop;

            next_uop->decode.in_fusion = true;
            next_uop->decode.is_fusion_head = false;
            next_uop->decode.fusion_size = 2;
            next_uop->decode.fusion_head = uop;
            next_uop->decode.fusion_next = nullptr;
        }
    }

    Mop->uop[0].decode.BOM = true;

    // XXX: No immediates for now
    Mop->decode.last_uop_index = Mop->decode.flow_length - 1;

#if 0
  flow_index = 0;
  while(flow_index < Mop->decode.flow_length)
  {
    struct uop_t * uop = &Mop->uop[flow_index];
    // uop->decode.FU_class = MD_OP_FUCLASS(uop->decode.op);
    /* Overwrite FU_class for OoO cores (done because only IO core has a dedicated AGU unit) */
    if (strcasecmp(knobs->model, "IO-DPM") && (uop->decode.FU_class == FU_AGEN))
      uop->decode.FU_class = FU_IEU;

  }
#endif

    flow_index = 0;
    while (flow_index < Mop->decode.flow_length) {
        struct uop_t* uop = &Mop->uop[flow_index];

        /* Fill mem repeater fields */
        if (uop->decode.is_load || uop->decode.is_sta || uop->decode.is_std) {
            uop->oracle.is_sync_op = core->fetch->fake_insn && !spec_mode;
            uop->oracle.is_repeated =
                core->current_thread->in_critical_section || uop->oracle.is_sync_op;
            if (uop->oracle.is_sync_op && uop->decode.is_std)
                core->num_signals_in_pipe++;
        }

        /* Mark fences in critical section as light.
         * XXX: This is a hack, so I don't hijack new x86 opcodes for light fences */
        if (uop->decode.is_fence && core->current_thread->in_critical_section)
            uop->decode.is_light_fence = true;

        /* XXX: execute the instruction */

        /* For loads, stas and stds, we need to grab virt_addr and mem_size from feeder. */
        if (uop->decode.is_load || uop->decode.is_sta || uop->decode.is_std) {
            zesto_assert(!handshake->mem_buffer.empty(), nullptr);
            uop->oracle.virt_addr = handshake->mem_buffer.begin()->first;
            /* XXX: Pass mem_size instead of value in handshake. */
            uop->decode.mem_size = 1;

            zesto_assert(uop->oracle.virt_addr != 0 || uop->Mop->oracle.spec_mode, NULL);
            uop->oracle.phys_addr =
                xiosim::memory::v2p_translate(thread->asid, uop->oracle.virt_addr);
        }

        flow_index += 1;  // MD_INC_FLOW;
    }

    /* update register mappings, inter-uop dependencies */
    /* NOTE: this occurs in its own loop because the above loop
       may terminate prematurely if a bogus fetch condition is
       encountered. */
    flow_index = 0;
    while (flow_index < Mop->decode.flow_length) {
        struct uop_t* uop = &Mop->uop[flow_index];
        /* update back/fwd pointers between uop and parent */
        install_dependencies(uop);
        /* add self to register mapping (oracle's rename table) */
        install_mapping(uop);
        flow_index += 1;  // MD_INC_FLOW;
    }

#if 0
  /* if PC == NPC, we're still REP'ing, or we've encountered an instruction
   * we can't handle */
  if(thread->regs.regs_PC == thread->regs.regs_NPC) {
      assert(Mop->oracle.spec_mode || Mop->fetch.inst.rep || Mop->decode.op == NOP);
      /* If we can't handle isntruction, at least set NPC correctly, so that we don't corrupt fetch sequence */
      if(Mop->decode.op == NOP && !Mop->oracle.spec_mode) {
          ZTRACE_PRINT(core->id, "XXX: Ignoring unknown instruction at pc: %x\n", thread->regs.regs_PC);
          ZESTO_STAT(core->stat.oracle_unknown_insn++;)
          assert(core->fetch->invalid);
          Mop->fetch.pred_NPC = thread->regs.regs_NPC;
          Mop->fetch.inst.len = thread->regs.regs_NPC - thread->regs.regs_PC;
      }
  }
#endif

    /* Mark EOM -- counting REP iterations as separate instructions */
    Mop->uop[Mop->decode.last_uop_index].decode.EOM = true;

    /* Magic instructions: fake NOPs go to a special magic ALU with a
     * configurable latency. Convenient to simulate various fixed-function HW. */
    if (x86::is_nop(Mop) && core->fetch->fake_insn && !spec_mode) {
        zesto_assert(Mop->decode.last_uop_index == 0, NULL);
        Mop->uop[0].decode.is_nop = false;
        Mop->uop[0].decode.FU_class = FU_MAGIC;
    }

    update_stats(Mop);

    /* commit this inst to the MopQ */
    MopQ_tail = modinc(MopQ_tail, MopQ_size);  //(MopQ_tail + 1) % MopQ_size;
    MopQ_num++;
    if (Mop->oracle.spec_mode)
        MopQ_spec_num++;

    current_Mop = Mop;

#ifdef ZTRACE
    ztrace_print(Mop);
#endif

    /* For traps, make sure pipeline has drained, halting fetch until so. */
    if (Mop->decode.is_trap) {
        core->current_thread->consumed = false;
        return nullptr;
    }

    return Mop;
}

void core_oracle_t::update_stats(struct Mop_t* const Mop) {
    if (!Mop->oracle.spec_mode)
        core->current_thread->stat.num_insn++;
    ZESTO_STAT(core->stat.oracle_total_insn++;)

    if (xed_decoded_inst_get_category(&Mop->decode.inst) == XED_CATEGORY_CALL)
        ZESTO_STAT(core->stat.oracle_total_calls++;)

    for (size_t i = 0; i < Mop->decode.flow_length; i++) {
        struct uop_t* uop = Mop->uop + i;
        if (uop->decode.is_imm)
            continue;

        if ((!uop->decode.in_fusion) || uop->decode.is_fusion_head) {
            Mop->stat.num_uops++;
            ZESTO_STAT(core->stat.oracle_total_uops++;)
        }
        Mop->stat.num_eff_uops++;
        ZESTO_STAT(core->stat.oracle_total_eff_uops++;)

        if (uop->decode.is_ctrl) {
            Mop->stat.num_branches++;
            ZESTO_STAT(core->stat.oracle_total_branches++;)
            if (!spec_mode)
                ZESTO_STAT(core->stat.oracle_num_branches++;)
        }

        if (uop->decode.is_load || uop->decode.is_std) {
            Mop->stat.num_refs++;
            ZESTO_STAT(core->stat.oracle_total_refs++;)
            if (!spec_mode)
                ZESTO_STAT(core->stat.oracle_num_refs++;)
            if (uop->decode.is_load) {
                Mop->stat.num_loads++;
                ZESTO_STAT(core->stat.oracle_total_loads++;)
                if (!spec_mode)
                    ZESTO_STAT(core->stat.oracle_num_loads++;)
            }
        }
    }
}

/* After calling oracle-exec, you need to first call this function
   to tell the oracle that you are in fact done with the previous
   Mop.  This may occur due to interruptions half-way through fetch
   processing (e.g., instruction spilt across cache lines). */
void core_oracle_t::consume(const struct Mop_t* const Mop) {
    assert(Mop == current_Mop);
    current_Mop = NULL;
}

void core_oracle_t::commit_uop(struct uop_t* const uop) {
    /* clean up idep/odep ptrs */
    struct odep_t* odep = uop->exec.odep_uop;
    while (odep) {
        struct odep_t* next = odep->next;
        zesto_assert(odep->uop, (void)0);
        odep->uop->exec.idep_uop[odep->op_num] = NULL;
        core->return_odep_link(odep);
        odep = next;
    }
    uop->exec.odep_uop = NULL;

    /* remove self from register mapping */
    commit_mapping(uop);

    /* clear oracle's back/fwd pointers between uop and children */
    commit_dependencies(uop);
}

/* This is called by the backend to inform the oracle that the pipeline has
   completed processing (committed) the entire Mop. */
void core_oracle_t::commit(const struct Mop_t* const commit_Mop) {
    struct Mop_t* Mop = &MopQ[MopQ_head];

    if (MopQ_num <= 0) /* nothing to commit */
        fatal("attempt to commit when MopQ is empty");

    zesto_assert(Mop == commit_Mop, (void)0);

    /* TODO: add checker support */

    assert(Mop->oracle.spec_mode == 0); /* can't commit wrong path insts! */

    Mop->valid = false;

    MopQ_head = modinc(MopQ_head, MopQ_size);  //(MopQ_head + 1) % MopQ_size;
    MopQ_num--;
    assert(MopQ_num >= 0);
    Mop->clear_uops();

    shadow_MopQ->pop();
    assert(shadow_MopQ->size() >= 0);
}

/* Undo the effects of the single Mop.  This function only affects the ISA-level
   state.  Bookkeeping for the MopQ and other core-level structures has to be
   dealt with separately. */
void core_oracle_t::undo(struct Mop_t* const Mop, bool nuke) {
    struct thread_t* thread = core->current_thread;
    /* walk uop list backwards, undoing each operation's effects */
    for (int i = Mop->decode.flow_length - 1; i >= 0; i--) {
        struct uop_t* uop = &Mop->uop[i];
        uop->exec.action_id = core->new_action_id(); /* squashes any in-flight loads/stores */

        /* collect stats */
        if (uop->decode.EOM) {
            if (!Mop->oracle.spec_mode) {
                thread->stat.num_insn--; /* one less oracle instruction executed */
            }
            ZESTO_STAT(core->stat.oracle_inst_undo++;)
        }

        if (uop->decode.is_imm)
            continue;
        else {
            /* one less oracle uop executed */
            if (!uop->decode.in_fusion || uop->decode.is_fusion_head) {
                ZESTO_STAT(core->stat.oracle_uop_undo++;)
            }
            ZESTO_STAT(core->stat.oracle_eff_uop_undo++;)
        }

        /* remove self from register mapping */
        undo_mapping(uop);

        /* clear back/fwd pointers between uop and parent */
        undo_dependencies(uop);
    }

    Mop->fetch.jeclear_action_id = core->new_action_id();
}

/* recover the oracle's state right up to Mop (but don't undo Mop) */
void core_oracle_t::recover(const struct Mop_t* const Mop) {
    std::stack<struct Mop_t*> to_delete;
    int idx = moddec(MopQ_tail, MopQ_size);  //(MopQ_tail-1+MopQ_size) % MopQ_size;

    ZTRACE_PRINT(
        core->id, "Recovery at MopQ_num: %d; shadow_MopQ_num: %d\n", MopQ_num, shadow_MopQ->size());

    while (Mop != &MopQ[idx]) {
        if (idx == MopQ_head)
            fatal("ran out of Mop's before finding requested MopQ recovery point");

        /* Flush not caused by branch misprediction - nuke */
        bool nuke = /*!spec_mode &&*/ !MopQ[idx].oracle.spec_mode;

        ZTRACE_PRINT(core->id,
                     "Undoing Mop @ PC: %x, nuke: %d, num_Mops_nuked: %d\n",
                     MopQ[idx].fetch.PC,
                     nuke,
                     num_Mops_before_feeder());

        undo(&MopQ[idx], nuke);
        MopQ[idx].valid = false;
        to_delete.push(&MopQ[idx]);
        if (MopQ[idx].fetch.bpred_update) {
            core->fetch->bpred->flush(MopQ[idx].fetch.bpred_update);
            core->fetch->bpred->return_state_cache(MopQ[idx].fetch.bpred_update);
            MopQ[idx].fetch.bpred_update = NULL;
        }

        if (!nuke) {
            zesto_assert(shadow_MopQ->back()->flags.speculative, (void)0);
            shadow_MopQ->pop_back();
            zesto_assert(shadow_MopQ->size() > 0, (void)0);
        }

        MopQ_num--;
        if (MopQ[idx].oracle.spec_mode)
            MopQ_spec_num--;
        MopQ_tail = idx;
        idx = moddec(idx, MopQ_size);  //(idx-1+MopQ_size) % MopQ_size;
    }

    while (!to_delete.empty()) {
        struct Mop_t* Mop_r = to_delete.top();
        to_delete.pop();
        Mop_r->clear_uops();
    }

    ZTRACE_PRINT(core->id,
                 "Recovering to fetchPC: %x; nuked_Mops: %d \n",
                 Mop->fetch.PC,
                 num_Mops_before_feeder());

    spec_mode = Mop->oracle.spec_mode;
    core->fetch->feeder_NPC = Mop->oracle.NextPC;

    /* Force simulation to re-check feeder if needed */
    core->current_thread->consumed = true;

    current_Mop = NULL;
}

/* flush everything after Mop */
void core_oracle_t::pipe_recover(struct Mop_t* const Mop, const md_addr_t New_PC) {
    struct core_knobs_t* knobs = core->knobs;
    if (knobs->fetch.jeclear_delay)
        core->fetch->jeclear_enqueue(Mop, New_PC);
    else {
        if (Mop->fetch.bpred_update)
            core->fetch->bpred->recover(Mop->fetch.bpred_update,
                                        (New_PC != (Mop->fetch.PC + Mop->fetch.inst.len)));
        this->recover(Mop);
        core->commit->recover(Mop);
        core->exec->recover(Mop);
        core->alloc->recover(Mop);
        core->decode->recover(Mop);
        core->fetch->recover(New_PC);
    }
}

/* flush everything including the Mop; restart fetching with this
   Mop again. */
void core_oracle_t::pipe_flush(struct Mop_t* const Mop) {
    const int prev_Mop_index =
        moddec(Mop - MopQ, MopQ_size);  //((Mop - MopQ) - 1 + MopQ_size) % MopQ_size;
    /* I don't think there are any uop flows that can cause intra-Mop violations */
    zesto_assert(MopQ_num > 1 && (Mop != &MopQ[MopQ_head]), (void)0);
    struct Mop_t* const prev_Mop = &MopQ[prev_Mop_index];
    /* CALLs/RETNs where the PC is loaded from memory, and that load is involved
       in a store-load ordering violation, can cause a branch (target) mispredict
       recovery from being properly taken care of (partly due to the fact that
       our uop flow does not act on the recovery until the lasp uop of the flow,
       which is typically *not* the load).  So to get around that, we just patch
       up the predicted NPC. */
    prev_Mop->fetch.pred_NPC = prev_Mop->oracle.NextPC;

    pipe_recover(prev_Mop, prev_Mop->fetch.pred_NPC);
}

/* like oracle recover, but empties out the entire pipeline, wrong-path or not */
void core_oracle_t::complete_flush(void) {
    std::stack<struct Mop_t*> to_delete;
    int idx = moddec(MopQ_tail, MopQ_size);  //(MopQ_tail-1+MopQ_size) % MopQ_size;
    while (MopQ_num) {
        assert(MopQ[idx].valid);
        undo(&MopQ[idx], false);
        MopQ[idx].valid = false;
        to_delete.push(&MopQ[idx]);
        if (MopQ[idx].fetch.bpred_update) {
            core->fetch->bpred->flush(MopQ[idx].fetch.bpred_update);
            core->fetch->bpred->return_state_cache(MopQ[idx].fetch.bpred_update);
            MopQ[idx].fetch.bpred_update = NULL;
        }

        MopQ_num--;
        if (MopQ[idx].oracle.spec_mode)
            MopQ_spec_num--;
        MopQ_tail = idx;
        idx = moddec(idx, MopQ_size);  //(idx-1+MopQ_size) % MopQ_size;
    }

    while (!shadow_MopQ->empty())
        shadow_MopQ->pop();

    while (!to_delete.empty()) {
        struct Mop_t* Mop_r = to_delete.top();
        to_delete.pop();
        Mop_r->clear_uops();
    }

    assert(MopQ_head == MopQ_tail);
    assert(shadow_MopQ->empty());

    spec_mode = false;
    current_Mop = NULL;
    /* Force simulation to re-check feeder if needed */
    core->current_thread->consumed = true;
}

grab_result_t core_oracle_t::grab_feeder_state(handshake_container_t* handshake,
                                      bool allocate_shadow,
                                      bool check_pc_mismatch) {
    // This usually happens when we insert fake instructions from pin.
    // Just use the feeder PC since the instruction context is from there.
/*    if (check_pc_mismatch && core->fetch->PC != handshake->pc) {
        if (handshake->flags.real && !core->fetch->prev_insn_fake) {
            ZTRACE_PRINT(
                core->id,
                "PIN->PC (0x%x) different from fetch->PC (0x%x). Overwriting with Pin value!\n",
                handshake->pc,
                core->fetch->PC);
        }
        core->fetch->PC = handshake->pc;
    }
*/
    ZTRACE_PRINT(core->id, "MopQ_num: %d, shadow_MopQ_num: %d\n",
                 MopQ_num, shadow_MopQ->size());
    handshake_container_t* new_handshake = nullptr;

    /* If we want a speculative handshake. */
    if (spec_mode || // We're already speculating
        core->fetch->PC != core->fetch->feeder_NPC) { // We're about to speculate

        /* Feeder didn't give us a speculative handshake. */
        if (!handshake->flags.speculative) {
            /* We'll manifacture ourselves a NOP. */
            new_handshake = new handshake_container_t();
            new_handshake->pc = core->fetch->PC;
            new_handshake->npc = core->fetch->PC + 1;
            new_handshake->tpc = core->fetch->PC + 1;
            new_handshake->flags.brtaken = false;
            new_handshake->flags.speculative = true;
            new_handshake->flags.real = true;
            new_handshake->flags.valid = true;
            new_handshake->asid = core->current_thread->asid;
            new_handshake->ins[0] = 0x90; // NOP
            handshake = new_handshake;
            ZTRACE_PRINT(core->id, "fake handshake -> PC: %x\n", core->fetch->PC);
        }

        /* We have a spec handshake, it's business as usual. */
    } else {
    /* We don't want a spec handshake. */
        /* Feeder gave us one anyway, we'll drop it. */
        if (handshake->flags.speculative) {
            return HANDSHAKE_NOT_NEEDED;
        }
    }

    core->fetch->feeder_PC = handshake->pc;
    core->fetch->prev_insn_fake = core->fetch->fake_insn;
    core->fetch->fake_insn = !handshake->flags.real;
    core->current_thread->asid = handshake->asid;
    /* Potentially change HELIX critical section state if instruction is fake. */
    if (!handshake->flags.real) {
        core->current_thread->in_critical_section = handshake->flags.in_critical_section;
    }

    /* Store a shadow handshake for recovery purposes */
    if (allocate_shadow) {
        zesto_assert(!shadow_MopQ->full(), ALL_GOOD);
        handshake_container_t* shadow_handshake = shadow_MopQ->get_buffer();
        /* Copy hadnshake to shadow. */
        new (shadow_handshake) handshake_container_t(*handshake);

        shadow_MopQ->push_done();
    }

    if (new_handshake) {
        delete new_handshake;
        return HANDSHAKE_NOT_CONSUMED;
    }

    return ALL_GOOD;
}

handshake_container_t* core_oracle_t::get_shadow_Mop(const struct Mop_t* Mop) {
    int Mop_ind = get_index(Mop);
    int from_head = (Mop_ind - MopQ_head) & (MopQ_size - 1);
    handshake_container_t* res = shadow_MopQ->get_item(from_head);
    zesto_assert(res->flags.valid, NULL);
    return res;
}

/* Dump instructions starting from most recent */
void core_oracle_t::trace_in_flight_ops(void) {
#ifdef ZTRACE
    if (MopQ_num == 0)
        return;

    ztrace_print(core->id, "===== TRACE OF IN-FLIGHT INS ====");

    /* Walk MopQ from most recent Mop */
    int idx = MopQ_tail;
    struct Mop_t* Mop;
    do {
        idx = moddec(idx, MopQ_size);
        Mop = &MopQ[idx];

        ztrace_print(Mop);
        ztrace_Mop_timing(Mop);
        ztrace_print(core->id, "");

        for (size_t i = 0; i < Mop->decode.flow_length; i++) {
            struct uop_t* uop = &Mop->uop[i];
            if (uop->decode.is_imm)
                continue;

            ztrace_uop_ID(uop);
            ztrace_uop_alloc(uop);
            ztrace_uop_timing(uop);
            ztrace_print(core->id, "");
        }

    } while (idx != MopQ_head);
#endif
}

unsigned int core_oracle_t::num_non_spec_Mops(void) const {
    int result = MopQ_num - MopQ_spec_num;
    zesto_assert(result >= 0, 0);
    return result;
}

unsigned int core_oracle_t::num_Mops_before_feeder(void) const {
    return shadow_MopQ->size() - MopQ_num;//num_non_spec_Mops();
}

/**************************************/
/* PROTECTED METHODS/MEMBER-FUNCTIONS */
/**************************************/

/* ORACLE DEPENDENCY MAP:
   The oracle keeps track of the equivalent of a register renaming
   table.  For each uop, the following functions add the uop to the
   output dependency list (odep list) of each of its input
   dependencies that the oracle still knows about (i.e., each parent
   uop that has not yet committed from the oracle), and the adds
   this uop into the mapping table as the most recent producer of
   the output register.  These pointers are used to implement the
   edges of the dataflow dependency graph which is used instead of
   explicit register renaming et al. to make the simulator a little
   bit faster. */

/* adds uop as most recent producer of its output(s) */
void core_oracle_t::install_mapping(struct uop_t* const uop) {
    for (size_t i = 0; i < MAX_ODEPS; i++) {
        auto produced_reg = uop->decode.odep_name[i];
        if (produced_reg == XED_REG_INVALID)
            continue;

        dep_map[produced_reg].push_back(uop);
    }
}

/* Called when a uop commits; removes uop from list of producers. */
void core_oracle_t::commit_mapping(const struct uop_t* const uop) {
    for (size_t i = 0; i < MAX_ODEPS; i++) {
        auto produced_reg = uop->decode.odep_name[i];
        if (produced_reg == XED_REG_INVALID)
            continue;

        /* if you're committing this, it better be the oldest producer */
        zesto_assert(dep_map[produced_reg].front() == uop, (void)0);
        dep_map[produced_reg].pop_front();
    }
}

/* Called when a uop is undone (during pipe flush); removes uop from
   list of producers.  Difference is commit removes from the head,
   undo removes from the tail. */
void core_oracle_t::undo_mapping(const struct uop_t* const uop) {
    for (size_t i = 0; i < MAX_ODEPS; i++) {
        auto produced_reg = uop->decode.odep_name[i];
        if (produced_reg == XED_REG_INVALID)
            continue;

        /* map can be empty if we undo before having added ourselves */
        if (dep_map[produced_reg].empty())
            continue;

        /* if you're undoing this, it better be the youngest producer */
        zesto_assert(dep_map[produced_reg].back() == uop, (void)0);
        dep_map[produced_reg].pop_back();
    }
}

/* Installs pointers back to the uop's parents, and installs odep
   pointers from the parents forward to this uop.  (Build uop's list
   of parents, add uop to each parent's list of children.) */
void core_oracle_t::install_dependencies(struct uop_t* const uop) {
    for (size_t i = 0; i < MAX_IDEPS; i++) {
        /* get pointers to parent uops */
        auto reg_name = uop->decode.idep_name[i];
        if (reg_name == XED_REG_INVALID)
            continue;

        /* parent has already committed */
        if (dep_map[reg_name].empty())
            continue;

        /* parent is the most recent producer of my operand */
        struct uop_t* parent_uop = dep_map[reg_name].back();

        /* make sure parent is older than me */
        assert(parent_uop->Mop->oracle.seq <= uop->Mop->oracle.seq);
        uop->oracle.idep_uop[i] = parent_uop;

        /* install pointers from parent to this uop (add to parent's odep list) */
        struct odep_t* odep = core->get_odep_link();
        odep->next = parent_uop->oracle.odep_uop;
        odep->uop = uop;
        odep->aflags = false;  // XXX
        odep->op_num = i;
        parent_uop->oracle.odep_uop = odep;
    }
}

/* Cleans up dependency pointers between uop and PARENTS. */
void core_oracle_t::undo_dependencies(struct uop_t* const uop) {
    for (size_t i = 0; i < MAX_IDEPS; i++) {
        struct uop_t* parent = uop->oracle.idep_uop[i];
        if (parent == nullptr)
            continue;

        struct odep_t* odep = parent->oracle.odep_uop;
        struct odep_t* odep_prev = nullptr;
        while (odep) {
            if ((odep->uop == uop) && ((size_t)odep->op_num == i)) {
                if (odep_prev)
                    odep_prev->next = odep->next;
                else
                    parent->oracle.odep_uop = odep->next;
                core->return_odep_link(odep);
                break;
            }
            odep_prev = odep;
            odep = odep->next;
        }
        /* if I point back at a parent, the parent should point fwd to me. */
        assert(odep);
        /* remove my back pointer */
        uop->oracle.idep_uop[i] = nullptr;
    }
}

/* Cleans up dependency pointers between uop and CHILDREN. */
void core_oracle_t::commit_dependencies(struct uop_t* const uop) {
    struct odep_t* odep = uop->oracle.odep_uop;

    while (odep) {
        struct odep_t* next = odep->next;

        /* remove from child's idep vector */
        assert(odep->uop->oracle.idep_uop[odep->op_num] == uop);
        odep->uop->oracle.idep_uop[odep->op_num] = nullptr;

        odep->uop = nullptr;
        odep->op_num = -1;
        core->return_odep_link(odep);

        odep = next;
    }
    uop->oracle.odep_uop = nullptr;
}
