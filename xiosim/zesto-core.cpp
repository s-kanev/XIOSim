/* zesto-core.cpp - Zesto core (single pipeline) class
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

#include "misc.h"
#include "stats.h"
#include "synchronization.h"

#include "zesto-structs.h"
#include "zesto-oracle.h"
#include "zesto-fetch.h"
#include "zesto-decode.h"
#include "zesto-alloc.h"
#include "zesto-exec.h"
#include "zesto-commit.h"

#include "zesto-core.h"

/* CONSTRUCTOR */
core_t::core_t(const int core_id)
    : knobs(NULL)
    , id(core_id)
    , sim_cycle(0)
    , active(false)
    , last_active_cycle(0)
    , ns_passed(0.0)
    , finished_cycle(false)
    , in_critical_section(false)
    , num_emergency_recoveries(0)
    , last_emergency_recovery_count(0)
    , oracle(NULL)
    , fetch(NULL)
    , decode(NULL)
    , alloc(NULL)
    , exec(NULL)
    , commit(NULL)
    , global_action_id(0)
    , odep_free_pool(NULL)
    , odep_free_pool_debt(0) {
    memzero(&memory, sizeof(memory));
    memzero(&stat, sizeof(stat));
}

/* assign a new, unique id */
seq_t core_t::new_action_id(void) {
    global_action_id++;
    return global_action_id;
}

/* Alloc/dealloc of the linked-list container nodes */
struct odep_t* core_t::get_odep_link(void) {
    struct odep_t* p = NULL;
    if (odep_free_pool) {
        p = odep_free_pool;
        odep_free_pool = p->next;
    } else {
        p = (struct odep_t*)calloc(1, sizeof(*p));
        if (!p)
            fatal("couldn't calloc an odep_t node");
    }
    assert(p);
    p->next = NULL;
    odep_free_pool_debt++;
    return p;
}

void core_t::return_odep_link(struct odep_t* const p) {
    p->next = odep_free_pool;
    odep_free_pool = p;
    p->uop = NULL;
    odep_free_pool_debt--;
    /* p->next used for free list, will be cleared on "get" */
}

void core_t::reg_common_stats(xiosim::stats::StatsDatabase* sdb) {
    bool is_DPM = strcasecmp(knobs->model, "STM") != 0;
    stat_reg_note(sdb, "\n#### TOP LEVEL CORE STATS ####");
    auto& core_cycle_st =
            stat_reg_core_counter(sdb, true, id, "sim_cycle",
                                  "total number of cycles when last instruction (or uop) committed",
                                  &stat.final_sim_cycle, 0, TRUE, NULL);
    stat_reg_core_counter(sdb, true, id, "commit_insn", "total number of instructions committed",
                          &stat.commit_insn, 0, TRUE, NULL);
    stat_reg_core_counter(sdb, true, id, "commit_uops", "total number of uops committed",
                          &stat.commit_uops, 0, TRUE, NULL);
    if (is_DPM) {
        stat_reg_core_counter(sdb, true, id, "commit_eff_uops",
                              "total number of effective uops committed", &stat.commit_eff_uops, 0,
                              TRUE, NULL);
    }
    auto sim_elapsed_time_st = stat_find_stat<double>(sdb, "sim_time");
    stat_reg_core_formula(sdb, true, id, "effective_frequency",
                          "effective frequency in MHz (XXX: assumes core was always active)",
                          core_cycle_st / *sim_elapsed_time_st, NULL);
}

void core_t::reg_stats(xiosim::stats::StatsDatabase* sdb) {
    reg_common_stats(sdb);
    oracle->reg_stats(sdb);
    fetch->reg_stats(sdb);
    decode->reg_stats(sdb);
    alloc->reg_stats(sdb);
    exec->reg_stats(sdb);
    commit->reg_stats(sdb);
}
