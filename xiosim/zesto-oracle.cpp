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

#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stack>
#include <sstream>


#include "host.h"
#include "misc.h"
#include "memory.h"
#include "decode.h"
#include "stats.h"
#include "uop_cracker.h"

#include "zesto-core.h"
#include "zesto-oracle.h"
#include "zesto-fetch.h"
#include "zesto-bpred.h"
#include "zesto-decode.h"
#include "zesto-alloc.h"
#include "zesto-exec.h"
#include "zesto-commit.h"
#include "ztrace.h"

using namespace std;

static size_t get_MopQ_size(const core_knobs_t* const knobs) {
    /* MopQ should be large enough to support all in-flight
       instructions.  We assume one entry per "slot" in the machine
       (even though in uop-based structures, multiple slots may
       correspond to a single Mop), and then add a few and round up
       just in case. */
    int temp_MopQ_size = knobs->commit.ROB_size + knobs->alloc.depth * knobs->alloc.width +
                         knobs->decode.uopQ_size + knobs->decode.depth * knobs->decode.width +
                         knobs->fetch.IQ_size + knobs->fetch.depth * knobs->fetch.width +
                         knobs->fetch.byteQ_size + 64;

    return 1 << ((int)ceil(log(temp_MopQ_size) / log(2.0)));
}

/* CONSTRUCTOR */
core_oracle_t::core_oracle_t(struct core_t* const arg_core)
    : spec_mode(false)
    , Mop_seq(0)
    , MopQ(NULL)
    , MopQ_head(0)
    , MopQ_tail(0)
    , MopQ_non_spec_tail(0)
    , MopQ_num(0)
    , MopQ_size(get_MopQ_size(arg_core->knobs))
    , current_Mop(NULL)
    , MopQ_spec_num(0)
    , drain_pipeline(false)
    , shadow_MopQ(get_MopQ_size(arg_core->knobs))
    , iclass_histogram(XED_ICLASS_LAST, 0)
    , iform_histogram(XED_IFORM_LAST, 0)
    , consumed(true) {
    core = arg_core;

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
}

core_oracle_t::~core_oracle_t() {
    for (int i = 0; i < MopQ_size; i++) {
        MopQ[i].clear();
    }
    free(MopQ);
}

/* register oracle-related stats in the stat-database (sdb) */
void core_oracle_t::reg_stats(xiosim::stats::StatsDatabase* sdb) {
    int coreID = core->id;

    stat_reg_note(sdb, "\n#### ORACLE STATS ####");
    auto sim_cycle_st = stat_find_core_stat<tick_t>(sdb, coreID, "sim_cycle");
    assert(sim_cycle_st);

    auto& oracle_total_insn_st = stat_reg_core_counter(
            sdb, true, coreID, "oracle_total_insn",
            "total number of instructions executed by oracle, including misspec",
            &core->stat.oracle_total_insn, 0, true, NULL);
    auto& oracle_insn_undo_st = stat_reg_core_counter(
            sdb, false, coreID, "oracle_insn_undo",
            "total number of instructions undone by oracle (misspeculated insts)",
            &core->stat.oracle_inst_undo, 0, true, NULL);
    stat_reg_core_counter(sdb, true, coreID, "oracle_unknown_insn",
                          "total number of unsupported instructions turned into NOPs by oracle",
                          &core->stat.oracle_unknown_insn, 0, true, NULL);
    stat_reg_core_formula(sdb, true, coreID, "oracle_num_insn",
                          "number of instructions executed by oracle",
                          oracle_total_insn_st - oracle_insn_undo_st, "%12.0f");
    auto& oracle_total_uops_st =
            stat_reg_core_counter(sdb, true, coreID, "oracle_total_uops",
                                  "total number of uops executed by oracle, including misspec",
                                  &core->stat.oracle_total_uops, 0, true, NULL);
    auto& oracle_uop_undo_st = stat_reg_core_counter(sdb, false, coreID, "oracle_uop_undo",
                                                     "total number of uops undone by oracle",
                                                     &core->stat.oracle_uop_undo, 0, true, NULL);
    stat_reg_core_formula(sdb, true, coreID, "oracle_num_uops", "number of uops executed by oracle",
                          oracle_total_uops_st - oracle_uop_undo_st, "%12.0f");
    auto& oracle_total_eff_uops_st = stat_reg_core_counter(
            sdb, true, coreID, "oracle_total_eff_uops",
            "total number of effective uops executed by oracle, including misspec",
            &core->stat.oracle_total_eff_uops, 0, true, NULL);
    auto& oracle_eff_uop_undo_st =
            stat_reg_core_counter(sdb, false, coreID, "oracle_eff_uop_undo",
                                  "total number of effective uops undone by oracle",
                                  &core->stat.oracle_eff_uop_undo, 0, true, NULL);
    stat_reg_core_formula(sdb, true, coreID, "oracle_num_eff_uops",
                          "number of effective uops executed by oracle",
                          oracle_total_eff_uops_st - oracle_eff_uop_undo_st, "%12.0f");

    stat_reg_core_formula(sdb, true, coreID, "oracle_IPC", "IPC at oracle",
                          (oracle_total_insn_st - oracle_insn_undo_st) / *sim_cycle_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "oracle_uPC", "IPC at oracle",
                          (oracle_total_uops_st - oracle_uop_undo_st) / *sim_cycle_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "oracle_euPC", "IPC at oracle",
                          (oracle_total_eff_uops_st - oracle_eff_uop_undo_st) / *sim_cycle_st,
                          NULL);

    stat_reg_core_formula(sdb, true, coreID, "oracle_total_IPC",
                          "IPC at oracle, including wrong-path",
                          oracle_total_insn_st / *sim_cycle_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "oracle_total_uPC",
                          "uPC at oracle, including wrong-path",
                          oracle_total_uops_st / *sim_cycle_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "oracle_total_euPC",
                          "euPC at oracle, including wrong-path",
                          oracle_total_eff_uops_st / *sim_cycle_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "avg_oracle_flowlen", "uops per instruction at oracle",
                          (oracle_total_uops_st - oracle_uop_undo_st) /
                                  (oracle_total_insn_st - oracle_insn_undo_st),
                          NULL);
    stat_reg_core_formula(sdb, true, coreID, "avg_oracle_eff_flowlen",
                          "uops per instruction at oracle",
                          (oracle_total_eff_uops_st - oracle_eff_uop_undo_st) /
                                  (oracle_total_insn_st - oracle_insn_undo_st),
                          NULL);
    stat_reg_core_formula(sdb, true, coreID, "avg_oracle_total_flowlen",
                          "uops per instruction at oracle, including wrong-path",
                          oracle_total_uops_st / oracle_total_insn_st, NULL);

    stat_reg_core_formula(sdb, true, coreID, "avg_oracle_total_eff_flowlen",
                          "effective uops per instruction at oracle, including wrong-path",
                          oracle_total_eff_uops_st / oracle_total_insn_st, NULL);

    auto& oracle_num_refs_st =
            stat_reg_core_counter(sdb, true, coreID, "oracle_num_refs",
                                  "total number of loads and stores executed by oracle",
                                  &core->stat.oracle_num_refs, 0, true, NULL);
    auto& oracle_num_loads_st = stat_reg_core_counter(sdb, true, coreID, "oracle_num_loads",
                                                      "total number of loads executed by oracle",
                                                      &core->stat.oracle_num_loads, 0, true, NULL);
    stat_reg_core_formula(sdb, true, coreID, "oracle_num_stores",
                          "total number of stores executed by oracle",
                          oracle_num_refs_st - oracle_num_loads_st, "%12.0f");
    stat_reg_core_counter(sdb, true, coreID, "oracle_num_branches",
                          "total number of branches executed by oracle",
                          &core->stat.oracle_num_branches, 0, true, NULL);

    auto& oracle_total_refs_st = stat_reg_core_counter(
            sdb, true, coreID, "oracle_total_refs",
            "total number of loads and stores executed by oracle, including wrong-path",
            &core->stat.oracle_total_refs, 0, true, NULL);
    auto& oracle_total_loads_st =
            stat_reg_core_counter(sdb, true, coreID, "oracle_total_loads",
                                  "total number of loads executed by oracle, including wrong-path",
                                  &core->stat.oracle_total_loads, 0, true, NULL);
    stat_reg_core_formula(sdb, true, coreID, "oracle_total_stores",
                          "total number of stores executed by oracle, including wrong-path",
                          oracle_total_refs_st - oracle_total_loads_st, "%12.0f");
    stat_reg_core_counter(sdb, true, coreID, "oracle_total_branches",
                          "total number of branches executed by oracle, including wrong-path",
                          &core->stat.oracle_total_branches, 0, true, NULL);
    stat_reg_core_counter(sdb, true, coreID, "oracle_total_calls",
                          "total number of function calls executed by oracle, including wrong-path",
                          &core->stat.oracle_total_calls, 0, true, NULL);
    reg_core_queue_occupancy_stats(sdb, coreID, "MopQ", &core->stat.MopQ_occupancy,
                                   &core->stat.MopQ_empty_cycles, &core->stat.MopQ_full_cycles);
    stat_reg_core_counter(sdb, true, coreID, "oracle_emergy_recoveries",
                          "number of times this thread underwent an emergency recovery",
                          &core->num_emergency_recoveries, 0, false, NULL);
    stat_reg_core_counter(sdb, true, coreID, "feeder_handshakes",
                          "number of handshakes coming in from feeder",
                          &core->stat.feeder_handshakes, 0, true, NULL);
    stat_reg_core_counter(sdb, true, coreID, "handshakes_dropped",
                          "number of handshakes dropped that were needlessly speculative",
                          &core->stat.handshakes_dropped, 0, true, NULL);
    stat_reg_core_counter(sdb, true, coreID, "oracle_handshakes_buffered",
                          "number of handshakes buffered in the shadow MopQ",
                          &core->stat.handshakes_buffered, 0, true, NULL);
    stat_reg_core_counter(sdb, true, coreID, "oracle_nop_handshakes",
                          "number of fake NOP handshakes oracle produced",
                          &core->stat.handshake_nops_produced, 0, true, NULL);
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

static xed_iclass_enum_t check_replacements(struct Mop_t* Mop) {
    /* The magic instruction have all kinds of crazy things: multiple uops,
     * memory operands, etc. While it's not too late, turn them into 1-byte
     * NOPs and remember which optimization we're representing. */
    if (x86::get_iclass(Mop) == XED_ICLASS_ADC) {
        Mop->fetch.code[0] = 0x90;
        x86::decode(Mop);
        x86::decode_flags(Mop);
        return XED_ICLASS_ADC;
    }
    return XED_ICLASS_INVALID;
}

struct Mop_t* core_oracle_t::exec(const md_addr_t requested_PC) {
    struct core_knobs_t* knobs = core->knobs;
    size_t flow_index = 0;
    struct Mop_t* Mop = NULL;

    /* We're about to execute a new Mop, we'd better not be draining. */
    zesto_assert(!drain_pipeline, nullptr);

    if (current_Mop) /* we already have a Mop */
    {
        assert(current_Mop->uop->timing.when_ready == TICK_T_MAX);
        assert(current_Mop->uop->timing.when_issued == TICK_T_MAX);
        assert(current_Mop->uop->timing.when_exec == TICK_T_MAX);
        assert(current_Mop->uop->timing.when_completed == TICK_T_MAX);
        return current_Mop;
    } else {
        Mop = &MopQ[MopQ_tail];
    }

    /* Stall fetch until we clear space in the MopQ. */
    if (MopQ_num >= MopQ_size) {
        return nullptr;
    }

    /* reset Mop state */
    Mop->clear();
    Mop->core = core;

    ZTRACE_PRINT(core->id, "reqPC: %" PRIxPTR ", feeder_NPC: %" PRIxPTR "\n", requested_PC,
                 core->fetch->feeder_NPC);
    /* go to next instruction */
    if (requested_PC != core->fetch->feeder_NPC)
        spec_mode = true;

    Mop->oracle.spec_mode = spec_mode;

    /* get the next instruction to execute from the shadow_MopQ */
    auto& handshake = shadow_MopQ.get_shadow_Mop(Mop);
    zesto_assert(spec_mode == handshake.flags.speculative, NULL);

    /* read encoding supplied by feeder */
    memcpy(&Mop->fetch.code, handshake.ins, xiosim::x86::MAX_ILEN);

    /* then decode the instruction */
    x86::decode(Mop);
    x86::decode_flags(Mop);

    xed_iclass_enum_t replacement_type = XED_ICLASS_INVALID;
    if (!handshake.flags.real)
        replacement_type = check_replacements(Mop);

    md_addr_t oracle_NPC = handshake.flags.brtaken ? handshake.tpc : handshake.npc;

    core->fetch->feeder_PC = handshake.pc;
    core->fetch->feeder_NPC = oracle_NPC;
    core->asid = handshake.asid;
    /* Potentially change HELIX critical section state if instruction is fake. */
    if (!handshake.flags.real) {
        core->in_critical_section = handshake.flags.in_critical_section;
    }

    Mop->fetch.PC = requested_PC;
    Mop->fetch.ftPC = handshake.npc;
    Mop->oracle.taken_branch = handshake.flags.brtaken;
    Mop->oracle.NextPC = oracle_NPC;

    if (Mop->decode.is_ctrl)
        Mop->decode.targetPC = handshake.tpc;

    /* set unique id */
    Mop->oracle.seq = Mop_seq++;

    /* Crack Mop into uops */
    x86::crack(Mop);

    /* Makes sure uops are owned and sequenced */
    for (size_t i = 0; i < Mop->decode.flow_length; i++) {
        Mop->uop[i].core = core;
        Mop->uop[i].decode.Mop_seq = Mop->oracle.seq;
        Mop->uop[i].decode.uop_seq = (Mop->oracle.seq << x86::UOP_SEQ_SHIFT) + i;
    }

    /* Fuse uops if the core model supports it. */
    for (size_t i = 0; i < Mop->decode.flow_length; i++) {
        struct uop_t* uop = &Mop->uop[i];

        if (knobs->decode.fusion_mode.matches(uop->decode.fusable)) {
            zesto_assert(i > 0, nullptr);
            struct uop_t* prev_uop = &Mop->uop[i - 1];

            /* Previous uop is the first in the fusion. */
            if (!prev_uop->decode.in_fusion) {
                zesto_assert(prev_uop->decode.fusion_head == nullptr, nullptr);
                prev_uop->decode.fusion_head = prev_uop;
                prev_uop->decode.fusion_size = 1;
                prev_uop->decode.in_fusion = true;
                prev_uop->decode.is_fusion_head = true;
            }

            prev_uop->decode.fusion_next = uop;
            uop->decode.in_fusion = true;
            uop->decode.fusion_next = nullptr;

            uop_t* fusion_head = prev_uop->decode.fusion_head;
            fusion_head->decode.fusion_size++;
            uop->decode.fusion_head = fusion_head;
        }
    }

    Mop->uop[0].decode.BOM = true;

    // XXX: No immediates for now
    Mop->decode.last_uop_index = Mop->decode.flow_length - 1;

    flow_index = 0;
    while (flow_index < Mop->decode.flow_length) {
        struct uop_t* uop = &Mop->uop[flow_index];

        /* Fill mem repeater fields */
        if (uop->decode.is_load || uop->decode.is_sta || uop->decode.is_std) {
            uop->oracle.is_sync_op = handshake.flags.helix_op && !spec_mode;
            uop->oracle.is_repeated = core->in_critical_section || uop->oracle.is_sync_op;
        }

        /* Mark fences in critical section as light.
         * XXX: This is a hack, so I don't hijack new x86 opcodes for light fences */
        if (uop->decode.is_lfence && core->in_critical_section)
            uop->decode.is_light_fence = true;

        /* For loads, stas and stds, we need to grab virt_addr and mem_size from feeder. */
        if (uop->decode.is_load || uop->decode.is_sta || uop->decode.is_std) {
            zesto_assert(!handshake.mem_buffer.empty(), nullptr);
            int mem_op_index = uop->oracle.mem_op_index;
            zesto_assert(mem_op_index >= 0 && mem_op_index < (int)handshake.mem_buffer.size(),
                         NULL);
            auto mem_access = handshake.mem_buffer[mem_op_index];
            uop->oracle.virt_addr = mem_access.first;
            uop->decode.mem_size = mem_access.second;
            if (uop->decode.is_pf)
                /* make sure SW prefetches never cross cache lines. */
                uop->decode.mem_size = 1;
            zesto_assert(uop->decode.mem_size <= (int)x86::MAX_MEMOP_SIZE, NULL);

            zesto_assert(uop->oracle.virt_addr != 0 || uop->Mop->oracle.spec_mode, NULL);
            uop->oracle.phys_addr =
                    xiosim::memory::v2p_translate(core->asid, uop->oracle.virt_addr);
        }

        flow_index += 1;  // MD_INC_FLOW;
    }

    /* update register mappings, inter-uop dependencies */
    flow_index = 0;
    while (flow_index < Mop->decode.flow_length) {
        struct uop_t* uop = &Mop->uop[flow_index];
        /* update back/fwd pointers between uop and parent */
        install_dependencies(uop);
        /* add self to register mapping (oracle's rename table) */
        install_mapping(uop);
        flow_index += 1;  // MD_INC_FLOW;
    }

    /* Mark EOM -- counting REP iterations as separate instructions */
    Mop->uop[Mop->decode.last_uop_index].decode.EOM = true;

    /* Magic instructions: fake NOPs go to a special magic ALU with a
     * configurable latency. Convenient to simulate various fixed-function HW. */
    if (replacement_type == XED_ICLASS_ADC) {
        zesto_assert(Mop->decode.last_uop_index == 0, NULL);
        Mop->uop[0].decode.is_nop = false;
        Mop->uop[0].decode.FU_class = FU_SAMPLING;
#ifdef ZTRACE
        ztrace_print(Mop, "Making sampling Mop magic.");
#endif
    } else if (x86::is_nop(Mop) && !handshake.flags.real && !spec_mode) {
        zesto_assert(Mop->decode.last_uop_index == 0, NULL);
        Mop->uop[0].decode.is_nop = false;
        Mop->uop[0].decode.FU_class = FU_MAGIC;
#ifdef ZTRACE
        ztrace_print(Mop, "Making Mop magic.");
#endif
    }

    update_stats(Mop);

    Mop->oracle.stopwatch_start = handshake.flags.is_profiling_start;
    Mop->oracle.stopwatch_stop = handshake.flags.is_profiling_stop;
    Mop->oracle.stopwatch_id = handshake.profile_id;

    /* commit this inst to the MopQ */
    MopQ_tail = modinc(MopQ_tail, MopQ_size);  //(MopQ_tail + 1) % MopQ_size;
    MopQ_num++;
    if (Mop->oracle.spec_mode) {
        MopQ_spec_num++;
    } else {
        MopQ_non_spec_tail = MopQ_tail;
    }

    current_Mop = Mop;

#ifdef ZTRACE
    ztrace_print(Mop);
#endif

    return Mop;
}

void core_oracle_t::update_stats(struct Mop_t* const Mop) {
    if (!Mop->oracle.spec_mode)
        core->stat.oracle_num_insn++;
    ZESTO_STAT(core->stat.oracle_total_insn++;)

    if (Mop->decode.opflags.CALL)
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

    /* TODO: we should use our fancy stats lib for these two.
     * For now, they print to a different file, and resolve labels
     * a little differently from what's supported. */
    auto iclass = xed_decoded_inst_get_iclass(&Mop->decode.inst);
    iclass_histogram[iclass]++;

    auto iform = xed_decoded_inst_get_iform_enum(&Mop->decode.inst);
    iform_histogram[iform]++;
}

/* After calling oracle-exec, you need to first call this function
   to tell the oracle that you are in fact done with the previous
   Mop.  This may occur due to interruptions half-way through fetch
   processing (e.g., instruction spilt across cache lines). */
void core_oracle_t::consume(const struct Mop_t* const Mop) {
    assert(Mop == current_Mop);
    current_Mop = NULL;
    consumed = true;

    /* For traps, start draining the pipeline, halting fetch from the next instruction on. */
    if (Mop->decode.is_trap) {
        ZTRACE_PRINT(core->id, "IT'S A TRAP!\n");
        drain_pipeline = true;
    }
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

    if (Mop->decode.is_trap) {
        zesto_assert(MopQ_num == 1, (void)0);
        drain_pipeline = false;
        /* Force simulation to re-check feeder */
        consumed = true;
    }

    Mop->valid = false;

    MopQ_head = modinc(MopQ_head, MopQ_size);  //(MopQ_head + 1) % MopQ_size;
    MopQ_num--;
    assert(MopQ_num >= 0);
    Mop->clear_uops();

    shadow_MopQ.pop();
}

/* Undo the effects of the single Mop.  This function only affects the ISA-level
   state.  Bookkeeping for the MopQ and other core-level structures has to be
   dealt with separately. */
void core_oracle_t::undo(struct Mop_t* const Mop, bool nuke) {
    /* walk uop list backwards, undoing each operation's effects */
    for (int i = Mop->decode.flow_length - 1; i >= 0; i--) {
        struct uop_t* uop = &Mop->uop[i];
        uop->exec.action_id = core->new_action_id(); /* squashes any in-flight loads/stores */

        /* collect stats */
        if (uop->decode.EOM) {
            if (!Mop->oracle.spec_mode) {
                core->stat.oracle_num_insn--; /* one less oracle instruction executed */
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

    ZTRACE_PRINT(core->id, "Recovery at MopQ_num: %d; shadow_MopQ_num: %d\n", MopQ_num,
                 shadow_MopQ.size());

    while (Mop != &MopQ[idx]) {
        if (idx == MopQ_head)
            fatal("ran out of Mop's before finding requested MopQ recovery point");

        /* Flush not caused by branch misprediction - nuke */
        bool nuke = !MopQ[idx].oracle.spec_mode;

        ZTRACE_PRINT(core->id,
                     "Undoing M:%" PRId64 " @ PC: %" PRIxPTR ", nuke: %d, num_Mops_nuked: %d\n",
                     MopQ[idx].oracle.seq,
                     MopQ[idx].fetch.PC,
                     nuke,
                     num_Mops_before_feeder());

        /* If we undo a trap, we're guaranteed there's no older trap, so we can
         * safely stop draining. */
        if (MopQ[idx].decode.is_trap)
            drain_pipeline = false;

        undo(&MopQ[idx], nuke);
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
        if (!MopQ[idx].oracle.spec_mode)
            MopQ_non_spec_tail = MopQ_tail;
        idx = moddec(idx, MopQ_size);  //(idx-1+MopQ_size) % MopQ_size;
    }

    while (!to_delete.empty()) {
        struct Mop_t* Mop_r = to_delete.top();
        to_delete.pop();
        Mop_r->clear_uops();
    }

    ZTRACE_PRINT(core->id,
                 "Recovering to fetchPC: %" PRIxPTR "; nuked_Mops: %u \n",
                 Mop->fetch.PC,
                 num_Mops_before_feeder());

    spec_mode = Mop->oracle.spec_mode;
    core->fetch->feeder_NPC = Mop->oracle.NextPC;

    /* Force simulation to re-check feeder if needed */
    consumed = true;

    current_Mop = NULL;
}

/* flush everything after Mop */
void core_oracle_t::pipe_recover(struct Mop_t* const Mop, const md_addr_t New_PC) {
    struct core_knobs_t* knobs = core->knobs;
    if (knobs->fetch.jeclear_delay)
        core->fetch->jeclear_enqueue(Mop, New_PC);
    else {
        if (Mop->fetch.bpred_update)
            core->fetch->bpred->recover(Mop->fetch.bpred_update, (New_PC != Mop->fetch.ftPC));
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
    /* XXX: There are. Things like indirect calls. We need a fix relaxing this assert */
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
        if (!MopQ[idx].oracle.spec_mode)
            MopQ_non_spec_tail = MopQ_tail;
        idx = moddec(idx, MopQ_size);  //(idx-1+MopQ_size) % MopQ_size;
    }

    while (!shadow_MopQ.empty())
        shadow_MopQ.pop();

    while (!to_delete.empty()) {
        struct Mop_t* Mop_r = to_delete.top();
        to_delete.pop();
        Mop_r->clear_uops();
    }

    assert(MopQ_head == MopQ_tail);
    assert(MopQ_head == MopQ_non_spec_tail);
    assert(shadow_MopQ.empty());

    spec_mode = false;
    drain_pipeline = false;
    current_Mop = NULL;
    /* Force simulation to re-check feeder if needed */
    consumed = true;
}

buffer_result_t core_oracle_t::buffer_handshake(handshake_container_t* handshake) {
    ZTRACE_PRINT(core->id, "Buffering %" PRIxPTR "\n", handshake->pc);
    /* If we want a speculative handshake. */
    if (spec_mode ||                                   // We're already speculating
        core->fetch->PC != core->fetch->feeder_NPC) {  // We're about to speculate

        /* But feeder isn't giving us one. */
        if (!handshake->flags.speculative) {
            /* We'll just manufacture ourselves a NOP. */
            handshake_container_t tmp_handshake = get_fake_spec_handshake();
            /* ... put it on the shadow_MopQ ... */
            shadow_MopQ.push_handshake(tmp_handshake);
            /* ... and make sure we're not done with the current one. */
            return HANDSHAKE_NOT_CONSUMED;
        }

        /* Feeder is speculating, but not from the PC we are.
         * We'll grab a NOP again. */
        if (handshake->pc != core->fetch->PC) {
            ZTRACE_PRINT(core->id,
                         "Spec FetchPC %" PRIxPTR " different from handshakePC %" PRIxPTR ".\n",
                         core->fetch->PC,
                         handshake->pc);
            handshake_container_t tmp_handshake = get_fake_spec_handshake();
            shadow_MopQ.push_handshake(tmp_handshake);
            return HANDSHAKE_NOT_CONSUMED;
        }
    } else {
        /* We're not speculating, but feeder gave us a speculative one.
         * For now, we'll just drop it. */
        if (handshake->flags.speculative) {
            core->stat.handshakes_dropped++;
            return HANDSHAKE_NOT_NEEDED;
        }

        /* This should happen very rarely, when handshake->npc was incorrect the previous
         * time around. E.g. a sysenter instruction. */
        if (handshake->pc != core->fetch->PC) {
            ZTRACE_PRINT(core->id,
                         "FetchPC %" PRIxPTR " different from handshakePC %" PRIxPTR
                         ". Correcting.\n",
                         core->fetch->PC,
                         handshake->pc);
            core->fetch->PC = handshake->pc;
            core->fetch->feeder_NPC = handshake->pc;
        }
    }

    core->stat.handshakes_buffered++;
    /* Store a shadow handshake for recovery purposes */
    zesto_assert(!shadow_MopQ.full(), ALL_GOOD);
    shadow_MopQ.push_handshake(*handshake);
    return ALL_GOOD;
}

/* Manufacture a fake NOP. */
handshake_container_t core_oracle_t::get_fake_spec_handshake() {
    handshake_container_t new_handshake;
    new_handshake.pc = core->fetch->PC;
    new_handshake.npc = core->fetch->PC + 3;
    new_handshake.tpc = core->fetch->PC + 3;
    new_handshake.flags.brtaken = false;
    new_handshake.flags.speculative = true;
    new_handshake.flags.real = true;
    new_handshake.flags.valid = true;
    new_handshake.asid = core->asid;
    new_handshake.ins[0] = 0x0f;  // 3-byte NOP
    new_handshake.ins[1] = 0x1f;  // 3-byte NOP
    new_handshake.ins[2] = 0x00;  // 3-byte NOP
    ZTRACE_PRINT(core->id, "fake handshake -> PC: %" PRIxPTR "\n", core->fetch->PC);
    core->stat.handshake_nops_produced++;
    return new_handshake;
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
    int result = shadow_MopQ.non_spec_size() - num_non_spec_Mops();
    zesto_assert(result >= 0, 0);
    return result;
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

void core_oracle_t::dump_instruction_histograms(const std::string iclass_prefix, const std::string iform_prefix) {
    std::stringstream ss;
    ss << "." << core->id;
    std::ofstream icof(iclass_prefix + ss.str());
    for (size_t i = 0; i < iclass_histogram.size(); i++) {
        if (iclass_histogram[i] > 0) {
            icof << std::setw(20) << std::left;
            icof << xed_iclass_enum_t2str(static_cast<xed_iclass_enum_t>(i)) << " ";
            icof << std::setw(12) << std::right << iclass_histogram[i] << std::endl;
        }
    }

    std::ofstream ifof(iform_prefix + ss.str());
    for (size_t i = 0; i < iform_histogram.size(); i++) {
        if (iform_histogram[i] > 0) {
            ifof << std::setw(36) << std::left;
            ifof << xed_iform_enum_t2str(static_cast<xed_iform_enum_t>(i)) << " ";
            ifof << std::setw(12) << std::right << iform_histogram[i] << std::endl;
        }
    }
}
