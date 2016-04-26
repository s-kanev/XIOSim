/* zesto-exec.cpp - Zesto execution stage class
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
 */

#include <cmath>
#include <cstddef>
#include <mutex>

#include "decode.h"
#include "memory.h"
#include "misc.h"
#include "regs.h"
#include "stats.h"
#include "synchronization.h"
#include "uop_cracker.h"
#include "ztrace.h"

#include "zesto-structs.h"
#include "zesto-cache.h"
#include "zesto-noc.h"
#include "zesto-repeater.h"
#include "zesto-core.h"
#include "zesto-oracle.h"
#include "zesto-alloc.h"
#include "zesto-exec.h"
#include "zesto-commit.h"
#include "zesto-memdep.h"
#include "zesto-coherence.h"
#include "zesto-uncore.h"
#include "helix.h"

/* default constructor */
core_exec_t::core_exec_t(struct core_t* const arg_core)
    : last_completed(0)
    , core(arg_core) {}

/* default destructor */
core_exec_t::~core_exec_t()
{
}

/* update deadlock watchdog timestamp */
void core_exec_t::update_last_completed(tick_t now)
{
  last_completed = now;
  core->commit->deadlocked = false;
}

enum cache_command core_exec_t::get_STQ_request_type(const struct uop_t * const uop)
{
  if(!uop->oracle.is_sync_op)
    return CACHE_WRITE;

  return is_addr_helix_signal(uop->oracle.virt_addr) ? CACHE_SIGNAL : CACHE_WAIT;
}

int core_exec_t::get_fp_penalty(const struct uop_t * const uop)
{
  bool freg_output = x86::is_freg(uop->decode.odep_name[0]);
  return (freg_output ^ uop->decode.is_fpop) ? core->knobs->exec.fp_penalty : 0;
}

extern int min_coreID;

/* load in all definitions */
#include "xiosim/ZPIPE-exec.list.h"


std::unique_ptr<class core_exec_t> exec_create(const char * exec_opt_string, struct core_t * core)
{
#define ZESTO_PARSE_ARGS
#include "xiosim/ZPIPE-exec.list.h"

  fatal("unknown exec type \"%s\"",exec_opt_string);
#undef ZESTO_PARSE_ARGS
}

/* Helper to create all dcache and dTLB structures and attach them to core->memory. */
void core_exec_t::create_caches(bool create_TLBs) {
    struct core_knobs_t* knobs = core->knobs;

    /* note: caches must be instantiated from the level furthest from the core first (e.g., L2) */
    char name[256];
    int sets, assoc, linesize, latency, banks, bank_width, MSHR_entries, MSHR_WB_entries;
    char rp, ap, wp, wc;

    if (!strcasecmp(knobs->memory.DL2_opt_str, "none")) {
        core->memory.DL2 = nullptr;
        core->memory.DL2_bus = nullptr;
    } else {
        if (sscanf(knobs->memory.DL2_opt_str, "%[^:]:%d:%d:%d:%d:%d:%d:%c:%c:%c:%d:%d:%c", name,
                   &sets, &assoc, &linesize, &banks, &bank_width, &latency, &rp, &ap, &wp,
                   &MSHR_entries, &MSHR_WB_entries, &wc) != 13)
            fatal("invalid DL2 options: "
                  "<name:sets:assoc:linesize:banks:bank-width:latency:repl-policy:alloc-policy:"
                  "write-policy:num-MSHR:WB-buffers:write-combining>\n\t(%s)",
                  knobs->memory.DL2_opt_str);
        /* per-core DL2 */
        core->memory.DL2 =
                cache_create(core, name, CACHE_READWRITE, sets, assoc, linesize, rp, ap, wp, wc,
                             banks, bank_width, latency, MSHR_entries, MSHR_WB_entries, 1,
                             uncore->LLC.get(), uncore->LLC_bus.get(),
                             knobs->memory.DL2_magic_hit_rate, knobs->memory.DL2_MSHR_cmd);
        prefetchers_create(core->memory.DL2.get(), knobs->memory.DL2_pf);

        /* per-core L2 bus (between L1 and L2) */
        core->memory.DL2_bus =
                bus_create("DL2_bus", core->memory.DL2->linesize, &core->memory.DL2->sim_cycle, 1);
    }

    /* per-core DL1 */
    if (sscanf(knobs->memory.DL1_opt_str, "%[^:]:%d:%d:%d:%d:%d:%d:%c:%c:%c:%d:%d:%c", name, &sets,
               &assoc, &linesize, &banks, &bank_width, &latency, &rp, &ap, &wp, &MSHR_entries,
               &MSHR_WB_entries, &wc) != 13)
        fatal("invalid DL1 options: "
              "<name:sets:assoc:linesize:banks:bank-width:latency:repl-policy:alloc-policy:write-"
              "policy:num-MSHR:WB-buffers:write-combining>\n\t(%s)",
              knobs->memory.DL1_opt_str);

    struct cache_t* next_level = (core->memory.DL2) ? core->memory.DL2.get() : uncore->LLC.get();
    struct bus_t* next_bus = (core->memory.DL2) ? core->memory.DL2_bus.get() : uncore->LLC_bus.get();
    core->memory.DL1 =
            cache_create(core, name, CACHE_READWRITE, sets, assoc, linesize, rp, ap, wp, wc, banks,
                         bank_width, latency, MSHR_entries, MSHR_WB_entries, 1, next_level,
                         next_bus, knobs->memory.DL1_magic_hit_rate, knobs->memory.DL1_MSHR_cmd);
    prefetchers_create(core->memory.DL1.get(), knobs->memory.DL1_pf);
    core->memory.DL1->controller =
            controller_create(knobs->memory.DL1_controller_opt_str, core, core->memory.DL1.get());
    if (core->memory.DL2)
        core->memory.DL2->controller =
                controller_create(knobs->memory.DL2_controller_opt_str, core, core->memory.DL2.get());

    /* memory repeater, aka ring cache */
    core->memory.mem_repeater = repeater_create(core->knobs->exec.repeater_opt_str, core, "MR1",
                                                core->memory.DL1.get());

    /* DTLBs */
    if (!create_TLBs)
        return;

    /* DTLB2 */
    if (!strcasecmp(knobs->memory.DTLB2_opt_str, "none")) {
        core->memory.DTLB2 = nullptr;
        core->memory.DTLB_bus = nullptr;
    } else {
        core->memory.DTLB_bus = bus_create("DTLB_bus", 1, &core->sim_cycle, 1);

        if (sscanf(knobs->memory.DTLB2_opt_str, "%[^:]:%d:%d:%d:%d:%c:%d", name, &sets, &assoc,
                   &banks, &latency, &rp, &MSHR_entries) != 7)
            fatal("invalid DTLB2 options: <name:sets:assoc:banks:latency:repl-policy:num-MSHR>");

        /* on a complete TLB miss, go to the next level to simulate the traffic from a HW page-table
         * walker */
        core->memory.DTLB2 =
                cache_create(core, name, CACHE_READONLY, sets, assoc, 1, rp, 'w', 't', 'n', banks,
                             1, latency, MSHR_entries, 4, 1, next_level, next_bus, -1.0, nullptr);
    }

    /* DTLB */
    if (sscanf(knobs->memory.DTLB_opt_str, "%[^:]:%d:%d:%d:%d:%c:%d", name, &sets, &assoc, &banks,
               &latency, &rp, &MSHR_entries) != 7)
        fatal("invalid DTLB options: <name:sets:assoc:banks:latency:repl-policy:num-MSHR>");

    struct cache_t* next_TLB_level = (core->memory.DTLB2) ? core->memory.DTLB2.get() : uncore->LLC.get();
    struct bus_t* next_TLB_bus = (core->memory.DTLB2) ? core->memory.DTLB_bus.get() : uncore->LLC_bus.get();
    core->memory.DTLB =
            cache_create(core, name, CACHE_READONLY, sets, assoc, 1, rp, 'w', 't', 'n', banks, 1,
                         latency, MSHR_entries, 4, 1, next_TLB_level, next_TLB_bus, -1.0, nullptr);

    core->memory.DTLB->controller =
            controller_create(knobs->memory.DTLB_controller_opt_str, core, core->memory.DTLB.get());
    if (core->memory.DTLB2 != NULL)
        core->memory.DTLB2->controller =
                controller_create(knobs->memory.DTLB2_controller_opt_str, core, core->memory.DTLB2.get());
}

void core_exec_t::reg_dcache_stats(xiosim::stats::StatsDatabase* sdb) {
    stat_reg_note(sdb, "\n#### DATA CACHE STATS ####");
    cache_reg_stats(sdb, core, core->memory.DL1.get());
    cache_reg_stats(sdb, core, core->memory.DTLB.get());
    cache_reg_stats(sdb, core, core->memory.DTLB2.get());
    cache_reg_stats(sdb, core, core->memory.DL2.get());
}

void core_exec_t::step_dcaches() {
    std::lock_guard<XIOSIM_LOCK> l(cache_lock);
    if (core->memory.DTLB2)
        cache_process(core->memory.DTLB2.get());
    if (core->memory.DTLB)
        cache_process(core->memory.DTLB.get());
    if (core->memory.DL2)
        cache_process(core->memory.DL2.get());
    if (core->memory.DL1)
        cache_process(core->memory.DL1.get());
}
