/* zesto-cache.cpp - Zesto cache structure
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

#include <ctype.h>
#include <limits.h>
#include <cmath>

#include "memory.h"
#include "misc.h"
#include "stats.h"
#include "sim.h"

#include "zesto-core.h"
#include "zesto-cache.h"
#include "zesto-prefetch.h"
#include "zesto-dram.h"
#include "zesto-uncore.h"
#include "zesto-coherence.h"
#include "zesto-noc.h"

#include "zesto-exec.h"
#include "zesto-commit.h"

#define CACHE_STAT(x) {if(!cp->frozen) {x}}

#define GET_BANK(x) (((x)>>cp->bank_shift) & cp->bank_mask)
#define GET_MSHR_BANK(x) (((x)>>cp->bank_shift) & cp->MSHR_mask)

/* Cache lock should be acquired before any access to the shared
 * caches (including enqueuing requests from lower-level caches). */
XIOSIM_LOCK cache_lock;

struct cache_t * cache_create(
    struct core_t * const core,
    const char * const name,
    const bool read_only,
    const int sets,
    const int assoc,
    const int linesize,
    const char rp, /* replacement policy */
    const char ap, /* allocation policy */
    const char wp, /* write policy */
    const char wc, /* write combining */
    const int banks,
    const int bank_width, /* in bytes; i.e., the interleaving granularity */
    const int latency, /* in cycles */
    const int MSHR_size, /* MSHR size (per bank), in requests */
    const int MSHR_WB_size, /* MSHRs reserved for writebacks (per bank), in requests */
    const int MSHR_banks, /* number of MSHR banks */
    struct cache_t * const next_level_cache, /* e.g., for the DL1, this should point to the L2 */
    struct bus_t * const next_bus, /* e.g., for the DL1, this should point to the bus between DL1 and L2 */
    const float magic_hit_rate
    )
{
  int i;
  struct cache_t * cp = (struct cache_t*) calloc(1,sizeof(*cp));
  if(!cp)
    fatal("failed to calloc cache %s",name);

  enum repl_policy_t replacement_policy;
  enum alloc_policy_t allocate_policy;
  enum write_policy_t write_policy;
  int write_combining;

  cp->core = core;
  cp->sim_cycle = 0;

  switch(toupper(rp)) {
    case 'L': replacement_policy = REPLACE_LRU; break;
    case 'M': replacement_policy = REPLACE_MRU; break;
    case 'R': replacement_policy = REPLACE_RANDOM; break;
    case 'N': replacement_policy = REPLACE_NMRU; break;
    case 'P': replacement_policy = REPLACE_PLRU; break;
    case 'C': replacement_policy = REPLACE_CLOCK; break;
    default: fatal("unrecognized cache replacement policy");
  }
  switch(toupper(ap)) {
    case 'W': allocate_policy = WRITE_ALLOC; break;
    case 'N': allocate_policy = NO_WRITE_ALLOC; break;
    default: fatal("unrecognized cache allocation policy (W=write-alloc,N=no-write-alloc)");
  }
  switch(toupper(wp)) {
    case 'T': write_policy = WRITE_THROUGH; break;
    case 'B': write_policy = WRITE_BACK; break;
    default: fatal("unrecognized cache write policy (T=write-Through,B=write-Back)");
  }
  switch(toupper(wc)) {
    case 'C': write_combining = true; break;
    case 'N': write_combining = false; break;
    default: fatal("unrecognized cache write combining option (C=write-Combining,N=No-write-combining)");
  }

  cp->name = strdup(name);
  if(!cp->name)
    fatal("failed to strdup %s's name",name);
  cp->read_only = (enum read_only_t) read_only;
  cp->sets = sets;
  cp->assoc = assoc;
  cp->log2_assoc = (int)(ceil(log(assoc)/log(2.0)));
  cp->linesize = linesize;
  cp->replacement_policy = replacement_policy;
  cp->allocate_policy = allocate_policy;
  cp->write_policy = write_policy;
  cp->banks = banks;
  cp->bank_width = bank_width;
  cp->bank_mask = banks-1;
  cp->latency = latency;
  cp->write_combining = write_combining;
  cp->MSHR_size = MSHR_size;
  cp->MSHR_WB_size = MSHR_WB_size;
  cp->MSHR_banks = MSHR_banks;
  cp->MSHR_mask = MSHR_banks-1;
  cp->next_level = next_level_cache;
  cp->next_bus = next_bus;
  cp->check_for_work = true;
  cp->check_for_MSHR_fill_work = true;
  cp->magic_hit_rate = magic_hit_rate;

  if((cp->replacement_policy == REPLACE_PLRU) && (assoc & (assoc-1)))
    fatal("Tree-based PLRU only works when associativity is a power of two");
  if((cp->replacement_policy == REPLACE_CLOCK) && (assoc > 64))
    fatal("Clock-based PLRU only works when associativity is <= 64");

  /* bits to shift to get to the tag */
  cp->addr_shift = (int)ceil(log(linesize)/log(2.0));
  /* bits to shift to get the bank number */
  cp->bank_shift = (int)ceil(log(linesize)/log(2.0));

  if((sets & (sets-1)) != 0)
    fatal("%s cache sets must be power of two");
  if((linesize & (linesize-1)) != 0)
    fatal("%s linesize must be power of two");
  if((banks & (banks-1)) != 0)
    fatal("%s banks must be power of two");

  cp->blocks = (struct cache_line_t**) calloc(sets,sizeof(*cp->blocks));
  if(!cp->blocks)
    fatal("failed to calloc cp->blocks");
  for(i=0;i<sets;i++)
  {
    /* allocate as one big block! */
    struct cache_line_t * line = (struct cache_line_t*) calloc(assoc,sizeof(*line));
    memset(line,0,assoc*sizeof(*line)); // for some weird reason valgrind was reporting that "line" was being used uninitialized, despite being calloc'd above?
    if(!line)
      fatal("failed to calloc cache line for %s",name);

    for(int j=0;j<assoc;j++)
    {
      line[j].way = j;
      line[j].next = &line[j+1];
    }
    line[assoc-1].next = NULL;
    cp->blocks[i] = line;
  }

  cp->heap_size = 1 << ((int) rint(ceil(log(latency+1)/log(2.0))));

  cp->pipe = (struct cache_action_t**) calloc(banks,sizeof(*cp->pipe));
  cp->pipe_num = (int*) calloc(banks,sizeof(*cp->pipe_num));
  cp->fill_pipe = (struct cache_fill_t**) calloc(banks,sizeof(*cp->fill_pipe));
  cp->fill_num = (int*) calloc(banks,sizeof(*cp->fill_num));
  if(!cp->pipe || !cp->fill_pipe || !cp->pipe_num || !cp->fill_num)
    fatal("failed to calloc cp->pipe for %s",name);
  for(i=0;i<banks;i++)
  {
    cp->pipe[i] = (struct cache_action_t*) calloc(cp->heap_size,sizeof(**cp->pipe));
    if(!cp->pipe[i])
      fatal("failed to calloc cp->pipe[%d] for %s",i,name);
    cp->fill_pipe[i] = (struct cache_fill_t*) calloc(cp->heap_size,sizeof(**cp->fill_pipe));
    if(!cp->fill_pipe[i])
      fatal("failed to calloc cp->fill_pipe[%d] for %s",i,name);
  }

  cache_assert(MSHR_size - MSHR_WB_size > 0, NULL);

  cp->MSHR = (struct cache_action_t**) calloc(MSHR_banks,sizeof(*cp->MSHR));
  if(!cp->MSHR)
    fatal("failed to calloc cp->MSHR for %s",name);
  for(i=0;i<MSHR_banks;i++)
  {
    cp->MSHR[i] = (struct cache_action_t*) calloc(MSHR_size,sizeof(**cp->MSHR));
    if(!cp->MSHR[i])
      fatal("failed to calloc cp->MSHR[%d] for %s",i,name);
  }

  cp->MSHR_num = (int*) calloc(MSHR_banks,sizeof(*cp->MSHR_num));
  if(!cp->MSHR_num)
    fatal("failed to calloc cp->MSHR_num");
  cp->MSHR_num_pf = (int*) calloc(MSHR_banks,sizeof(*cp->MSHR_num_pf));
  if(!cp->MSHR_num_pf)
    fatal("failed to calloc cp->MSHR_num_pf");
  cp->MSHR_fill_num = (int*) calloc(MSHR_banks,sizeof(*cp->MSHR_fill_num));
  if(!cp->MSHR_fill_num)
    fatal("failed to calloc cp->MSHR_fill_num");
  cp->MSHR_unprocessed_num = (int*) calloc(MSHR_banks,sizeof(*cp->MSHR_unprocessed_num));
  if(!cp->MSHR_unprocessed_num)
    fatal("failed to calloc cp->MSHR_unprocessed_num");
  cp->MSHR_WB_num = (int*) calloc(MSHR_banks,sizeof(*cp->MSHR_num));
  if(!cp->MSHR_WB_num)
    fatal("failed to calloc cp->MSHR_num");

  if(!strcasecmp(name,"LLC"))
  {
    cp->stat.core_lookups = (counter_t*) calloc(system_knobs.num_cores,sizeof(*cp->stat.core_lookups));
    cp->stat.core_misses = (counter_t*) calloc(system_knobs.num_cores,sizeof(*cp->stat.core_misses));
    if(!cp->stat.core_lookups || !cp->stat.core_misses)
      fatal("failed to calloc cp->stat.core_{lookups|misses} for %s",name);
  }

  return cp;
}

void cache_reg_stats(xiosim::stats::StatsDatabase* sdb,
                     const struct core_t* const core,
                     struct cache_t* const cp) {
    using namespace xiosim::stats;

    if (!core)
        fatal("must provide a core for cache_reg_stats; for LLC, use LLC_reg_stats");
    if (!cp)
        return;

    int coreID = core->id;
    auto sim_cycle_st = stat_find_core_stat<tick_t>(sdb, coreID, "sim_cycle");
    auto commit_insn_st = stat_find_core_stat<counter_t>(sdb, coreID, "commit_insn");
    auto commit_uops_st = stat_find_core_stat<counter_t>(sdb, coreID, "commit_uops");
    auto commit_eff_uops_st = stat_find_core_stat<counter_t>(sdb, coreID, "commit_eff_uops");

    if (cp->read_only == CACHE_READWRITE) {
        auto& load_lookups_st = stat_reg_cache_counter(
                sdb, true, coreID, cp->name, "load_lookups", "number of load lookups in %s",
                &cp->stat.load_lookups, 0, true, NULL);
        auto& load_misses_st = stat_reg_cache_counter(sdb, true, coreID, cp->name, "load_misses",
                                                      "number of load misses in %s",
                                                      &cp->stat.load_misses, 0, true, NULL);
        stat_reg_cache_formula(sdb, true, coreID, cp->name, "load_miss_rate",
                               "load miss rate in %s", load_misses_st / load_lookups_st, "%12.4f");

        auto& store_lookups_st = stat_reg_cache_counter(
                sdb, true, coreID, cp->name, "store_lookups", "number of store lookups in %s",
                &cp->stat.store_lookups, 0, true, NULL);
        auto& store_misses_st = stat_reg_cache_counter(
                sdb, true, coreID, cp->name, "store_misses", "number of store misses in %s",
                &cp->stat.store_misses, 0, true, NULL);
        stat_reg_cache_formula(sdb, true, coreID, cp->name, "store miss rate",
                               "store miss rate in %s", store_misses_st / store_lookups_st,
                               "%12.4f");

        auto& writeback_lookups_st = stat_reg_cache_counter(
                sdb, true, coreID, cp->name, "writeback_lookups",
                "number of writeback lookups in %s", &cp->stat.writeback_lookups, 0, true, NULL);
        auto& writeback_misses_st = stat_reg_cache_counter(
                sdb, true, coreID, cp->name, "writeback_misses",
                "number of writeback misses in %s", &cp->stat.writeback_misses, 0, true, NULL);
        stat_reg_cache_formula(sdb, true, coreID, cp->name, "writeback_miss_rate",
                               "writeback miss rate in %s",
                               writeback_misses_st / writeback_lookups_st, "%12.4f");

        // These do not include prefetcher lookups yet.
        // TODO: Fix the names of these formulas and their descriptions.
        Formula total_lookups("total_lookups",
                              "total number of lookups in %s (excluding writebacks)", "%12.0f");
        Formula total_misses("total_misses", "total number of misses in %s (excluding writebacks)",
                             "%12.0f");
        Formula total_miss_rate("total_miss_rate", "total miss rate in %s (excluding writebacks)",
                                "%12.4f");
        total_lookups = load_lookups_st + store_lookups_st;
        total_misses = load_misses_st + store_misses_st;

        if (cp->num_prefetchers > 0) {
            auto& pf_lookups_st = stat_reg_cache_counter(sdb, true, coreID, cp->name, "pf_lookups",
                                   "number of prefetch lookups in %s", &cp->stat.prefetch_lookups,
                                   0, true, NULL);
            auto& pf_misses_st = stat_reg_cache_counter(sdb, true, coreID, cp->name, "pf_misses",
                                   "number of prefetch misses in %s", &cp->stat.prefetch_misses, 0,
                                   true, NULL);
            stat_reg_cache_formula(sdb, true, coreID, cp->name, "pf_miss_rate",
                                   "prefetch miss rate in %s", pf_misses_st / pf_lookups_st,
                                   "%12.4f");

            auto& pf_insertions_st = stat_reg_cache_counter(sdb, true, coreID, cp->name, "pf_insertions",
                                   "number of prefetched blocks inserted into %s",
                                   &cp->stat.prefetch_insertions, 0, true, NULL);
            auto& pf_useful_insertions_st = stat_reg_cache_counter(sdb, true, coreID, cp->name, "pf_useful_insertions",
                                   "number of prefetched blocks actually used in %s",
                                   &cp->stat.prefetch_useful_insertions, 0, true, NULL);
            stat_reg_cache_formula(sdb, true, coreID, cp->name, "pf_useful_rate",
                                   "rate of useful prefetches in %s",
                                   pf_useful_insertions_st / pf_insertions_st, "%12.4f");

            total_lookups += pf_lookups_st;
            total_misses += pf_misses_st;
            total_miss_rate = (load_misses_st + store_misses_st + pf_misses_st) /
                                 (load_lookups_st + store_lookups_st + pf_lookups_st);
        } else {
            // TODO: Add support for building Formulas from existing
            // Statistics. Until then, we have to define this formula like so.
            total_miss_rate = (load_misses_st + store_misses_st) /
                                 (load_lookups_st + store_lookups_st);
        }
        stat_reg_formula(sdb, total_lookups);
        stat_reg_formula(sdb, total_misses);
        stat_reg_formula(sdb, total_miss_rate);

        stat_reg_cache_formula(
                sdb, true, coreID, cp->name, "MPKI",
                "total miss rate in MPKI (no prefetches) for %s (misses/thousand cycles)",
                (load_misses_st + store_misses_st) / *commit_insn_st * Constant(1000), "%12.4f");
        stat_reg_cache_formula(sdb, true, coreID, cp->name, "MPKu",
                         "total miss rate in MPKu (no prefetches) for %s (misses/thousand cycles)",
                         (load_misses_st + store_misses_st) / *commit_uops_st * Constant(1000),
                         "%12.4f");
        stat_reg_cache_formula(sdb, true, coreID, cp->name, "MPKeu",
                         "total miss rate in MPKeu (no prefetches) for %s (misses/thousand cycles)",
                         (load_misses_st + store_misses_st) / *commit_eff_uops_st * Constant(1000),
                         "%12.4f");
        stat_reg_cache_formula(sdb, true, coreID, cp->name, "MPKC",
                         "total miss rate in MPKC (no prefetches) for %s (misses/thousand cycles)",
                         (load_misses_st + store_misses_st) / *sim_cycle_st * Constant(1000),
                         "%12.4f");
    } else {
        auto& lookups_st =
                stat_reg_cache_counter(sdb, true, coreID, cp->name, "lookups",
                                       "number of lookups in %s", &cp->stat.load_lookups, 0, true, NULL);
        auto& misses_st =
                stat_reg_cache_counter(sdb, true, coreID, cp->name, "misses",
                                       "number of misses in %s", &cp->stat.load_misses, 0, true, NULL);

        if (cp->num_prefetchers > 0) {
            auto& pf_lookups_st = stat_reg_cache_counter(sdb, true, coreID, cp->name, "pf_lookups",
                                   "number of prefetch lookups in %s", &cp->stat.prefetch_lookups,
                                   0, true, NULL);
            auto& pf_misses_st = stat_reg_cache_counter(sdb, true, coreID, cp->name, "pf_misses",
                                   "number of prefetch misses in %s", &cp->stat.prefetch_misses, 0,
                                   true, NULL);
            stat_reg_cache_formula(sdb, true, coreID, cp->name, "pf_miss_rate",
                                   "prefetch miss rate in %s", pf_misses_st / pf_lookups_st,
                                   "%12.4f");

            auto& pf_insertions_st = stat_reg_cache_counter(sdb, true, coreID, cp->name, "pf_insertions",
                                   "number of prefetched blocks inserted into %s",
                                   &cp->stat.prefetch_insertions, 0, true, NULL);
            auto& pf_useful_insertions_st = stat_reg_cache_counter(sdb, true, coreID, cp->name, "pf_useful_insertions",
                                   "number of prefetched blocks actually used in %s",
                                   &cp->stat.prefetch_useful_insertions, 0, true, NULL);
            stat_reg_cache_formula(sdb, true, coreID, cp->name, "pf_useful_rate",
                                   "rate of useful prefetches in %s",
                                   pf_useful_insertions_st / pf_insertions_st, "%12.4f");

            stat_reg_cache_formula(sdb, true, coreID, cp->name, "miss_rate",
                                   "miss rate in %s (no prefetches)", misses_st / lookups_st,
                                   "%12.4f");
            stat_reg_cache_formula(
                    sdb, true, coreID, cp->name, "total_miss_rate", "miss rate in %s",
                    (misses_st + pf_misses_st) / (lookups_st + pf_lookups_st), "%12.4f");
        } else {
            stat_reg_cache_formula(sdb, true, coreID, cp->name, "miss_rate",
                                   "miss rate in %s (no prefetches)", misses_st / lookups_st,
                                   "%12.4f");
        }

        stat_reg_cache_formula(sdb, true, coreID, cp->name, "MPKI",
                               "miss rate in MPKI (no prefetches) for %s",
                               (misses_st) / *commit_insn_st * Constant(1000), "%12.4f");
        stat_reg_cache_formula(sdb, true, coreID, cp->name, "MPKu",
                               "miss rate in MPKu (no prefetches) for %s",
                               (misses_st) / *commit_uops_st * Constant(1000), "%12.4f");
        stat_reg_cache_formula(sdb, true, coreID, cp->name, "MPKeu",
                               "miss rate in MPKeu (no prefetches) for %s",
                               (misses_st) / *commit_eff_uops_st * Constant(1000), "%12.4f");
        stat_reg_cache_formula(sdb, true, coreID, cp->name, "MPKC",
                               "miss rate in MPKC (no prefetches) for %s",
                               (misses_st) / *sim_cycle_st * Constant(1000), "%12.4f");
    }
    auto& MSHR_total_occupancy_st = stat_reg_cache_counter(
            sdb, false, coreID, cp->name, "MSHR_total_occupancy",
            "cumulative MSHR occupancy in %s", &cp->stat.MSHR_occupancy, 0, true, NULL);
    stat_reg_cache_formula(sdb, true, coreID, cp->name, "MSHR_avg_occupancy",
                           "average MSHR entries in use in %s",
                           MSHR_total_occupancy_st / *sim_cycle_st, "%12.4f");
    auto& MSHR_full_cycles_st = stat_reg_cache_counter(
            sdb, false, coreID, cp->name, "MSHR_full_cycles", "cycles MSHR was full in %s",
            &cp->stat.MSHR_full_cycles, 0, true, NULL);
    stat_reg_cache_formula(sdb, true, coreID, cp->name, "MSHR_full",
                           "fraction of time MSHRs are full in %s",
                           MSHR_full_cycles_st / *sim_cycle_st, "%12.4f");
    stat_reg_cache_counter(sdb, true, coreID, cp->name, "MSHR_combos",
                           "MSHR requests combined in %s", &cp->stat.MSHR_combos, 0, true, NULL);

    for (int i = 0; i < cp->num_prefetchers; i++)
        cp->prefetcher[i]->reg_stats(sdb, core);

    if (cp->controller)
        cp->controller->reg_stats(sdb);
}

void LLC_reg_stats(xiosim::stats::StatsDatabase* sdb, struct cache_t* const cp) {
    using namespace xiosim::stats;

    assert(cp->read_only == CACHE_READWRITE);

    auto sim_cycle_st = stat_find_stat<tick_t>(sdb, "sim_cycle");

    auto& LLC_load_lookups_st =
            stat_reg_counter(sdb, true, "LLC.load_lookups", "number of load lookups in LLC",
                             &cp->stat.load_lookups, 0, true, NULL);
    auto& LLC_load_misses_st =
            stat_reg_counter(sdb, true, "LLC.load_misses", "number of load misses in LLC",
                             &cp->stat.load_misses, 0, true, NULL);
    stat_reg_formula(sdb, true, "LLC.load_miss_rate", "load miss rate in LLC",
                     LLC_load_misses_st / LLC_load_lookups_st, "%12.4f");

    auto& LLC_store_lookups_st =
            stat_reg_counter(sdb, true, "LLC.store_lookups", "number of store lookups in LLC",
                             &cp->stat.store_lookups, 0, true, NULL);
    auto& LLC_store_misses_st =
            stat_reg_counter(sdb, true, "LLC.store_misses", "number of store misses in LLC",
                             &cp->stat.store_misses, 0, true, NULL);
    stat_reg_formula(sdb, true, "LLC.store_miss_rate", "store miss rate in LLC",
                     LLC_store_misses_st / LLC_store_lookups_st, "%12.4f");

    auto& LLC_writeback_lookups_st = stat_reg_counter(sdb, true, "LLC.writeback_lookups",
                                                      "number of writeback lookups in LLC",
                                                      &cp->stat.writeback_lookups, 0, true, NULL);
    auto& LLC_writeback_misses_st =
            stat_reg_counter(sdb, true, "LLC.writeback_misses", "number of writeback misses in LLC",
                             &cp->stat.writeback_misses, 0, true, NULL);
    stat_reg_formula(sdb, true, "LLC.writeback_miss_rate", "writeback miss rate in LLC",
                     LLC_writeback_misses_st / LLC_writeback_lookups_st, "%12.4f");

    Formula total_lookups("total_lookups", "total number of lookups in LLC (excluding writebacks)",
                          "%12.0f");
    Formula total_misses("total_misses", "total number of misses in LLC (excluding writebacks)",
                         "%12.0f");
    Formula total_miss_rate("total_miss_rate", "total miss rate in LLC (excluding writebacks)",
                            "%12.4f");

    total_lookups = LLC_load_lookups_st + LLC_store_lookups_st;
    total_misses = LLC_load_misses_st + LLC_store_misses_st;
    if (cp->num_prefetchers > 0) {
        auto& LLC_pf_lookups_st =
                stat_reg_counter(sdb, true, "LLC.pf_lookups", "number of prefetch lookups in LLC",
                                 &cp->stat.prefetch_lookups, 0, true, NULL);
        auto& LLC_pf_misses_st =
                stat_reg_counter(sdb, true, "LLC.pf_misses", "number of prefetch misses in LLC",
                                 &cp->stat.prefetch_misses, 0, true, NULL);
        stat_reg_formula(sdb, true, "LLC.pf_miss_rate", "prefetch miss rate in LLC",
                         LLC_pf_misses_st / LLC_pf_lookups_st, "%12.4f");

        auto& LLC_pf_insertions_st = stat_reg_counter(
                sdb, true, "LLC.pf_insertions", "number of prefetched blocks inserted into LLC",
                &cp->stat.prefetch_insertions, 0, true, NULL);
        auto& LLC_pf_useful_insertions_st =
                stat_reg_counter(sdb, true, "LLC.pf_useful_insertions",
                                 "number of prefetched blocks actually used in LLC",
                                 &cp->stat.prefetch_useful_insertions, 0, true, NULL);
        stat_reg_formula(sdb, true, "LLC.pf_useful_rate", "rate of useful prefetches in LLC",
                         LLC_pf_useful_insertions_st / LLC_pf_insertions_st, "%12.4f");

        total_lookups += LLC_pf_lookups_st;
        total_misses += LLC_pf_misses_st;
        total_miss_rate = (LLC_load_misses_st + LLC_store_misses_st + LLC_pf_misses_st) /
                          (LLC_load_lookups_st + LLC_store_lookups_st + LLC_pf_lookups_st);

        if (system_knobs.num_cores > 1)
            stat_reg_formula(sdb, true, "LLC.total_MPKC", "MPKC for the LLC (including prefetches)",
                             (LLC_load_misses_st + LLC_store_misses_st + LLC_pf_misses_st) /
                                     *sim_cycle_st * Constant(1000),
                             "%12.4f");

    } else {
        // We register this stat even though we don't have a prefetcher because legacy.
        if (system_knobs.num_cores > 1)
            stat_reg_formula(sdb, true, "LLC.total_MPKC", "MPKC for the LLC (including prefetches)",
                             (LLC_load_misses_st + LLC_store_misses_st) / *sim_cycle_st *
                                     Constant(1000),
                             "%12.4f");
        total_miss_rate = (LLC_load_misses_st + LLC_store_misses_st) /
                          (LLC_load_lookups_st + LLC_store_lookups_st);
    }
    stat_reg_formula(sdb, total_lookups);
    stat_reg_formula(sdb, total_misses);
    stat_reg_formula(sdb, total_miss_rate);

    if (system_knobs.num_cores > 1) {
        stat_reg_formula(sdb, true, "LLC.MPKC", "MPKC for the LLC (no prefetches or writebacks)",
                         (LLC_load_misses_st + LLC_store_misses_st) / *sim_cycle_st *
                                 Constant(1000),
                         "%12.4f");
    }

    auto& LLC_MSHR_total_occupancy_st = stat_reg_counter(sdb, false, "LLC.MSHR_total_occupancy",
                                                         "cumulative MSHR occupancy in LLC",
                                                         &cp->stat.MSHR_occupancy, 0, true, NULL);
    stat_reg_formula(sdb, true, "LLC.MSHR_avg_occupancy", "average MSHR entries in use in LLC",
                     LLC_MSHR_total_occupancy_st / *sim_cycle_st, "%12.4f");
    auto& LLC_MSHR_full_cycles_st =
            stat_reg_counter(sdb, false, "LLC.MSHR_full_cycles", "cycles MSHR was full in LLC",
                             &cp->stat.MSHR_full_cycles, 0, true, NULL);
    stat_reg_formula(sdb, true, "LLC.MSHR_full", "fraction of time MSHRs are full in LLC",
                     LLC_MSHR_full_cycles_st / *sim_cycle_st, "%12.4f");

    stat_reg_counter(sdb, true, "LLC.MSHR_combos", "MSHR requests combined in LLC",
                     &cp->stat.MSHR_combos, 0, true, NULL);

    if (system_knobs.num_cores == 1) {
        auto commit_insn_st = stat_find_core_stat<counter_t>(sdb, 0, "commit_insn");
        stat_reg_counter(sdb, true, "LLC.lookups", "number of lookups in the LLC",
                         &cp->stat.core_lookups[0], 0, true, NULL);
        auto& LLC_misses_st =
                stat_reg_counter(sdb, true, "LLC.misses", "number of misses in the LLC",
                                 &cp->stat.core_misses[0], 0, true, NULL);
        stat_reg_formula(sdb, true, "LLC.MPKI", "MPKI for the LLC",
                         LLC_misses_st / *commit_insn_st * Constant(1000), "%12.4f");
        stat_reg_formula(sdb, true, "LLC.MPKC", "MPKC for the LLC",
                         LLC_misses_st / *sim_cycle_st * Constant(1000), "%12.4f");
    } else {
        for (int i = 0; i < system_knobs.num_cores; i++) {
            auto commit_insn_st = stat_find_core_stat<counter_t>(sdb, i, "commit_insn");
            auto core_sim_cycle_st = stat_find_core_stat<tick_t>(sdb, i, "sim_cycle");

            auto& LLC_lookups_st =
                    stat_reg_cache_counter(sdb, true, i, cp->name, "lookups",
                                           "number of lookups by core %d in shared %s cache",
                                           &cp->stat.core_lookups[i], 0, true, NULL, true);
            auto& LLC_misses_st =
                    stat_reg_cache_counter(sdb, true, i, cp->name, "misses",
                                           "number of misses by core %d in shared %s cache",
                                           &cp->stat.core_misses[i], 0, true, NULL, true);
            stat_reg_cache_formula(sdb, true, i, cp->name, "miss_rate",
                                   "miss rate by core %d in shared %s cache",
                                   LLC_misses_st / LLC_lookups_st, "%12.4f", true);
            stat_reg_cache_formula(sdb, true, i, cp->name, "MPKI",
                                   "MPKI by core %d in shared %s cache",
                                   LLC_misses_st / *commit_insn_st * Constant(1000), "%12.4f", true);
            stat_reg_cache_formula(sdb, true, i, cp->name, "MPKC",
                                   "MPKC by core %d in shared %s cache",
                                   LLC_misses_st / *core_sim_cycle_st * Constant(1000), "%12.4f", true);
        }
    }

    if (cp->prefetcher)
        for (int i = 0; i < cp->num_prefetchers; i++)
            cp->prefetcher[i]->reg_stats(sdb, NULL);

    if (cp->controller)
        cp->controller->reg_stats(sdb);
}

/* Called after fast-forwarding/warmup to clear out the stats */
void cache_reset_stats(struct cache_t * const cp)
{
  counter_t * core_lookups = cp->stat.core_lookups;
  counter_t * core_misses = cp->stat.core_misses;
  memzero(&cp->stat,sizeof(cp->stat));
  if(core_lookups)
  {
    cp->stat.core_lookups = core_lookups;
    cp->stat.core_misses = core_misses;
    memzero(cp->stat.core_lookups,system_knobs.num_cores*sizeof(*cp->stat.core_lookups));
    memzero(cp->stat.core_misses,system_knobs.num_cores*sizeof(*cp->stat.core_misses));
  }
}

/* Create an optional prefetch buffer so that prefetches get inserted here first
   before being promoted to the main cache. */
void prefetch_buffer_create(
    struct cache_t * const cp,
    const int num_entries)
{
  cp->PF_buffer_size = num_entries;
  for(int i=0;i<num_entries;i++)
  {
    struct prefetch_buffer_t * p = (struct prefetch_buffer_t*) calloc(1,sizeof(*p));
    if(!p)
      fatal("couldn't calloc prefetch buffer entry");
    p->addr = (md_paddr_t)-1;
    p->next = cp->PF_buffer;
    cp->PF_buffer = p;
  }
}

/* Create an optional prefetch filter that predicts whether a prefetch will be
   useful.  If it's predicted to not be, then don't bother inserting the
   prefetch into the cache.  TODO: this should be updated so that the prefetch
   doesn't even happen in the first place; the current implementation reduces
   cache pollution, but doesn't cut down on unnecessary bus utilization. */
void prefetch_filter_create(
    struct cache_t * const cp,
    const int num_entries,
    const int reset_interval)
{
  if(num_entries == 0)
    return;
  cp->PF_filter = (struct prefetch_filter_t*) calloc(1,sizeof(*cp->PF_filter));
  if(!cp->PF_filter)
    fatal("couldn't calloc prefetch filter");
  cp->PF_filter->num_entries = num_entries;
  cp->PF_filter->mask = num_entries-1;
  cp->PF_filter->reset_interval = reset_interval;

  cp->PF_filter->table = (char*) calloc(num_entries,sizeof(*cp->PF_filter->table));
  if(!cp->PF_filter->table)
    fatal("couldn't calloc prefetch filter table");
  for(int i=0;i<num_entries;i++)
    cp->PF_filter->table[i] = 3;
}

/* Returns true if the prefetch should get inserted into the cache */
static int prefetch_filter_lookup(
    struct cache_t * const cp,
    struct prefetch_filter_t * const p,
    const md_paddr_t addr)
{
  const tick_t cycle = cache_get_cycle(cp);
  if(cycle >= (p->last_reset + p->reset_interval))
  {
    for(int i=0;i<p->num_entries;i++)
      p->table[i] = 3;
    p->last_reset = cycle - (cycle % p->reset_interval);
    return true;
  }

  int index = addr & p->mask;
  return (p->table[index] >= 2);
}

/* Update the filter's predictor based on whether a prefetch was useful or not. */
static void prefetch_filter_update(
    struct cache_t * const cp,
    struct prefetch_filter_t * const p,
    const md_paddr_t addr,
    const int useful)
{
  const int index = addr & p->mask;
  const tick_t cycle = cache_get_cycle(cp);
  if(cycle > (p->last_reset + p->reset_interval))
  {
    for(int i=0;i<p->num_entries;i++)
      p->table[i] = 3;
    p->last_reset = cycle - (cycle % p->reset_interval);
  }
  if(useful)
  {
    if(p->table[index] < 3)
      p->table[index]++;
  }
  else
  {
    if(p->table[index] > 0)
      p->table[index]--;
  }
}


/* Check to see if a given address can be found in the cache.  This is only a "peek" function
   in that it does not update any hit/miss stats, although it does update replacement state. */
struct cache_line_t * cache_is_hit(
    struct cache_t * const cp,
    const enum cache_command cmd,
    const md_paddr_t addr,
    struct core_t * const core)
{
  const md_paddr_t block_addr = addr >> cp->addr_shift;
  const int index = block_addr & (cp->sets-1);
  struct cache_line_t * p = cp->blocks[index];
  struct cache_line_t * prev = NULL;

  /* Predefined hit rate for magic simulation. Doesn't properly maintain
   * replacement state, but hey, magic. */
  if(cp->magic_hit_rate != -1.0) {
    float r = random() / float(RAND_MAX);
    if (r < cp->magic_hit_rate) {
      assert(p != NULL);
      return p;
    }
    else
      return NULL;
  }

  while(p) /* search all of the ways */
  {
    if(p->valid && (p->tag == block_addr)) /* hit */
    {
      if(cmd != CACHE_WRITEBACK)
        switch(cp->replacement_policy)
        {
          case REPLACE_PLRU: /* tree-based pseudo-LRU */
          {
            int bitmask = cp->blocks[index][0].meta;
            const int way = p->way;
            for(int i=0;i<cp->log2_assoc;i++)
            {
              const int pos = (way >> (cp->log2_assoc-i)) + (1<<i);

              if((way>>(cp->log2_assoc-i-1)) & 1)
                bitmask |= (1<<pos);
              else
                bitmask &= ~(1<<pos);
            }
            cp->blocks[index][0].meta = bitmask;
          }
          XIOSIM_FALLTHROUGH;
            /* NO BREAK IN THE CASE STATEMENT HERE: Do the LRU ordering, too.
               This doesn't affect the behavior of the replacement policy, but
               for highly-associative caches it'll speed up any subsequent
               searches (assuming temporal locality) */

          case REPLACE_RANDOM: /* similar: random doesn't need it, but this can speed up searches */
          case REPLACE_LRU:
          case REPLACE_NMRU:
            {
              if(prev) /* insert back at front of list */
              {
                prev->next = p->next;
                p->next = cp->blocks[index];
                cp->blocks[index] = p;
              }
              break;
            }
          case REPLACE_MRU:
            {
              if(p->next) /* only move node if not already at end of list */
              {
                /* remove node */
                if(prev)
                  prev->next = p->next;
                else
                  cp->blocks[index] = p->next;

                /* go to end of list */
                prev = p->next;
                while(prev->next)
                  prev = prev->next;
                
                /* stick ourselves there */
                prev->next = p;
                p->next = NULL;
              }
              break;
            }
          case REPLACE_CLOCK:
          {
            cp->blocks[index][0].meta |= 1ULL<<p->way; /* set referenced bit */
            break;
          }
          default:
            fatal("policy not yet implemented");
        }

      if(cmd == CACHE_WRITE || cmd == CACHE_WRITEBACK)
      {
        cache_assert(cp->read_only != CACHE_READONLY,NULL);
        if(cp->write_policy == WRITE_BACK) /* write-thru doesn't have dirty lines */
          p->dirty = true;
      }

      return p;
    }
    prev = p;
    p = p->next;
  }

  /* miss */
  return NULL;
}

/* Check to see if a given address can be found in the cache.  This is a *true* "peek"
   function; the replacement state is not touched. */
static struct cache_line_t * cache_peek(
    const struct cache_t * const cp,
    const md_paddr_t addr)
{
  const md_paddr_t block_addr = addr >> cp->addr_shift;
  const int index = block_addr & (cp->sets-1);
  struct cache_line_t * p = cp->blocks[index];

  while(p) /* search all of the ways */
  {
    if(p->valid && (p->tag == block_addr)) /* hit */
    {
      return p;
    }
    p = p->next;
  }

  /* miss */
  return NULL;
}

/* Overwrite the victim line with a new block.  This function only gets called after
   the previous evictee (as determined by the replacement policy) has already been
   removed, which means there is at least one invalid line (which we use for the
   newly inserted line). */
void cache_insert_block(
    struct cache_t * const cp,
    const enum cache_command cmd,
    const md_paddr_t addr,
    core_t * const core)
{
  /* assumes block not already present */
  const md_paddr_t block_addr = addr >> cp->addr_shift;
  const int index = block_addr & (cp->sets-1);
  struct cache_line_t * p = cp->blocks[index];
  struct cache_line_t *prev = NULL;

  /* there had better be an invalid line now - cache_get_evictee should
     have already returned a line to be invalidated */
  while(p)
  {
    if(!p->valid)
      break;
    prev = p;
    p = p->next;
  }

  cache_assert(p,(void)0);
  p->tag = block_addr;
  p->core = core;
  p->valid = true;
  if(cmd == CACHE_WRITE || cmd == CACHE_WRITEBACK)
    p->dirty = true;
  else
    p->dirty = false;
  if(cmd == CACHE_PREFETCH)
  {
    p->prefetched = true;
    CACHE_STAT(cp->stat.prefetch_insertions++;)
  }

  if(cmd != CACHE_WRITEBACK) /* writebacks don't update replacement state */
  {
    switch(cp->replacement_policy)
    {
      case REPLACE_PLRU: /* tree-based pseudo-LRU */
      {
        int bitmask = cp->blocks[index][0].meta;
        const int way = p->way;
        for(int i=0;i<cp->log2_assoc;i++)
        {
          int pos = (way >> (cp->log2_assoc-i)) + (1<<i);

          if((way>>(cp->log2_assoc-i-1)) & 1)
            bitmask |= (1<<pos);
          else
            bitmask &= ~(1<<pos);
        }
        cp->blocks[index][0].meta = bitmask;
      }
      XIOSIM_FALLTHROUGH;
      /* same comment about case statement fall-through as in cache_is_hit() */
      case REPLACE_RANDOM:
      case REPLACE_LRU:
      case REPLACE_NMRU:
        if(prev) /* put to front of list */
        {
          prev->next = p->next;
          p->next = cp->blocks[index];
          cp->blocks[index] = p;
        }
        break;
      case REPLACE_MRU:
        if(p->next) /* only move node if not already at end of list */
        {
          /* remove node */
          if(prev)
            prev->next = p->next;
          else
            cp->blocks[index] = p->next;

          /* go to end of list */
          prev = p->next;
          while(prev->next)
            prev = prev->next;
          
          /* stick ourselves there */
          prev->next = p;
          p->next = NULL;
        }
        break;
      case REPLACE_CLOCK:
      {
        /* do not set referenced bit */
        break;
      }
      default:
        fatal("policy not yet implemented");
    }
  }
}

/* caller of get_evictee is responsible for writing back (if needed) and
   invalidating the entry prior to inserting a new block */
struct cache_line_t * cache_get_evictee(
    struct cache_t * const cp,
    const md_paddr_t addr,
    core_t * const core)
{
  int block_addr = addr >> cp->addr_shift;
  int index = block_addr & (cp->sets-1);
  struct cache_line_t * p = cp->blocks[index];

  switch(cp->replacement_policy)
  {
    /* this works for both LRU and MRU, since MRU just sorts its recency list backwards */
    case REPLACE_LRU:
    case REPLACE_MRU:
    {
      while(p)
      {
        if(!p->next || !p->valid) /* take any invalid line, else take the last one (LRU) */
          return p;

        p = p->next;
      }
      break;
    }
    case REPLACE_RANDOM:
    {
      /* use an invalid block if possible */
      while(p)
      {
        if(!p->valid) /* take any invalid line */
          return p;

        p = p->next;
      }

      if(!p) /* no invalid line, pick at random */
      {
        const int pos = random() % cp->assoc;
        p = cp->blocks[index];

        for(int i=0;i<pos;i++)
          p = p->next;

        return p;
      }

      break;
    }
    case REPLACE_NMRU:
    {
      /* use an invalid block if possible */
      while(p)
      {
        if(!p->valid) /* take any invalid line */
          return p;
        p = p->next;
      }

      if((cp->assoc > 1) && !p) /* no invalid line, pick at random from non-MRU */
      {
        const int pos = random() % (cp->assoc-1);
        p = cp->blocks[index];
        p = p->next; /* skip MRU */

        for(int i=0;i<pos;i++)
          p = p->next;
      }
      return p;
    }
    case REPLACE_PLRU:
    {
      int bitmask = cp->blocks[index][0].meta;
      int i;
      int node = 1;

      while(p)
      {
        if(!p->valid) /* take any invalid line */
          return p;
        p = p->next;
      }

      for(i=0;i<cp->log2_assoc;i++)
      {
        const int bit = (bitmask >> node) & 1;

        node = (node<<1) + !bit;
      }

      const int way = node & ~(1<<cp->log2_assoc);

      p = cp->blocks[index];
      for(i=0;i<cp->assoc;i++)
      {
        if(p->way==way)
          break;
        p = p->next;
      }

      return p;
    }
    case REPLACE_CLOCK:
    {
      int just_in_case = 0;

      while(1)
      {
        uint64_t way = cp->blocks[index][1].meta;
        struct cache_line_t * p = &cp->blocks[index][way];

        /* increment clock */
        cp->blocks[index][1].meta = modinc(way,cp->assoc); //(way+1) % cp->assoc;

        if(!p->valid) /* take any invalid line */
        {
          cp->blocks[index][0].meta &= ~(1ULL<<p->way); /* make sure referenced bit is clear */
          return p;
        }
        else if(!((cp->blocks[index][0].meta >> p->way) & 1)) /* not referenced */
        {
          return p;
        }
        else
        {
          cp->blocks[index][0].meta &= ~(1ULL<<p->way); /* clear referenced bit */
        }

        just_in_case++;
        if(just_in_case > (20*cp->assoc))
          fatal("Clock-PLRU has gone around twice without finding an evictee for %s",cp->name);
      }
    }
    default:
      fatal("policy not yet implemented");
  }

  fatal("unreachable code");
}

/* input state: assumes that the new node has just been inserted at
   insert_position and all remaining nodes obey the heap property */
static void cache_heap_balance(
    struct cache_action_t * const pipe,
    const int insert_position)
{
  int pos = insert_position;
  //struct cache_action_t tmp;
  while(pos > 1)
  {
    int parent = pos >> 1;
    if(pipe[parent].pipe_exit_time > pipe[pos].pipe_exit_time)
    {
      /*
      tmp = pipe[parent];
      pipe[parent] = pipe[pos];
      pipe[pos] = tmp;
      */
      memswap(&pipe[parent],&pipe[pos],sizeof(*pipe));
      pos = parent;
    }
    else
      return;
  }
}

/* input state: pipe_num is the number of elements in the heap
   prior to removing the root node. */
static void cache_heap_remove(
    struct cache_action_t * const pipe,
    const int pipe_num)
{
  if(pipe_num == 1) /* only one node to remove */
  {
    pipe[1].cb = NULL;
    pipe[1].pipe_exit_time = TICK_T_MAX;
    return;
  }

  pipe[1] = pipe[pipe_num]; /* move last node to root */
  //struct cache_action_t tmp;

  /* delete previous last node */
  pipe[pipe_num].cb = NULL;
  pipe[pipe_num].pipe_exit_time = TICK_T_MAX;

  /* push node down until heap property re-established */
  int pos = 1;
  while(1)
  {
    int Lindex = pos<<1;     // index of left child
    int Rindex = (pos<<1)+1; // index of right child
    tick_t myValue = pipe[pos].pipe_exit_time;

    tick_t Lvalue = TICK_T_MAX;
    tick_t Rvalue = TICK_T_MAX;
    if(Lindex < pipe_num) /* valid index */
      Lvalue = pipe[Lindex].pipe_exit_time;
    if(Rindex < pipe_num) /* valid index */
      Rvalue = pipe[Rindex].pipe_exit_time;

    if(((myValue > Lvalue) && (Lvalue != TICK_T_MAX)) ||
       ((myValue > Rvalue) && (Rvalue != TICK_T_MAX)))
    {
      if(Rvalue == TICK_T_MAX) /* swap pos with L */
      {
        /* tmp = pipe[Lindex];
        pipe[Lindex] = pipe[pos];
        pipe[pos] = tmp; */
        memswap(&pipe[Lindex],&pipe[pos],sizeof(*pipe));
        return;
      }
      else if((myValue > Lvalue) && (Lvalue < Rvalue))
      {
        /* tmp = pipe[Lindex];
        pipe[Lindex] = pipe[pos];
        pipe[pos] = tmp; */
        memswap(&pipe[Lindex],&pipe[pos],sizeof(*pipe));
        pos = Lindex;
      }
      else /* swap pos with R */
      {
        /* tmp = pipe[Rindex];
        pipe[Rindex] = pipe[pos];
        pipe[pos] = tmp; */
        memswap(&pipe[Rindex],&pipe[pos],sizeof(*pipe));
        pos = Rindex;
      }
    }
    else
      return;
  }
}

/* Can a cache access get enqueued into the cache?  Returns true if it
   can.  Cases when it can't include, for example, more than one request
   per cycle goes to the same bank, or if a bank has been locked up. */
int cache_enqueuable(
    const struct cache_t * const cp,
    const int asid,
    const md_paddr_t addr)
{
  md_paddr_t paddr = xiosim::memory::v2p_translate(asid, addr);
  const int bank = GET_BANK(paddr);
  if(cp->pipe_num[bank] < cp->latency)
    return true;
  else
    return false;
}

/* Enqueue a cache access request to the cache.  Assumes you already
   called cache_enqueuable. */
void cache_enqueue(
    struct core_t * const core,
    struct cache_t * const cp,
    struct cache_t * const prev_cp,
    const enum cache_command cmd,
    const int asid,
    const md_addr_t PC,
    const md_paddr_t addr,
    const seq_t action_id,
    const int MSHR_bank,
    const int MSHR_index,
    void * const op,
    void (*const cb)(void *),
    void (*const miss_cb)(void *, int),
    bool (*const translated_cb)(void *,seq_t),
    seq_t (*const get_action_id)(void *),
    const bool prefetcher_hint)
{
  md_paddr_t paddr = xiosim::memory::v2p_translate(asid, addr);
  const int bank = GET_BANK(paddr);

  /* heap initial insertion position */
  const int insert_position = cp->pipe_num[bank]+1;
  cache_assert(cp->pipe[bank][insert_position].cb == NULL,(void)0);
  cache_assert(cb != NULL,(void)0);

  cp->pipe[bank][insert_position].core = core;
  cp->pipe[bank][insert_position].prev_cp = prev_cp;
  cp->pipe[bank][insert_position].cmd = cmd;
  cp->pipe[bank][insert_position].PC = PC;
  cp->pipe[bank][insert_position].paddr = paddr;
  cp->pipe[bank][insert_position].op = op;
  cp->pipe[bank][insert_position].action_id = action_id;
  cp->pipe[bank][insert_position].MSHR_bank = MSHR_bank;
  cp->pipe[bank][insert_position].MSHR_index = MSHR_index;
  cp->pipe[bank][insert_position].cb = cb;
  cp->pipe[bank][insert_position].miss_cb = miss_cb;
  cp->pipe[bank][insert_position].translated_cb = translated_cb;
  cp->pipe[bank][insert_position].get_action_id = get_action_id;
  cp->pipe[bank][insert_position].when_started = cache_get_cycle(cp);;
  cp->pipe[bank][insert_position].when_returned = TICK_T_MAX;
  cp->pipe[bank][insert_position].type = MSHR_MISS;
  cp->pipe[bank][insert_position].prefetcher_hint = prefetcher_hint;
  cp->pipe[bank][insert_position].pipe_exit_time = cache_get_cycle(cp)+cp->latency;
  cp->pipe[bank][insert_position].miss_cb_invoked = false;

  assert(insert_position < cp->heap_size);
  cache_heap_balance(cp->pipe[bank],insert_position);

  cp->pipe_num[bank]++;
  //cache_assert(cp->pipe_num[bank] <= cp->latency,(void)0);

  cp->check_for_work = true;
  cp->check_for_pipe_work = true;

}

void dummy_callback(void * p)
{
  /* this is just a place holder for cast-out writebacks
     in a write-back cache */
}

/* Returns true if at least one MSHR entry is free/available. */
static inline int MSHR_available(
    const struct cache_t * const cp,
    const md_paddr_t paddr)
{
  const int bank = GET_MSHR_BANK(paddr);
  return cp->MSHR_num[bank] < (cp->MSHR_size - cp->MSHR_WB_size);
}

/* Returns true if at least one MSHR writeback entry is free/available. */
static inline int MSHR_WB_available(
    const struct cache_t * const cp,
    const md_paddr_t paddr)
{
  const int bank = GET_MSHR_BANK(paddr);
  return cp->MSHR_WB_num[bank] < cp->MSHR_WB_size;
}

/* Returns a pointer to a free MSHR entry; assumes you already called
   MSHR_available to make sure there's room */
static struct cache_action_t * MSHR_allocate(
    struct cache_t * const cp,
    const md_paddr_t paddr,
    const enum cache_command cmd)
{
  const int bank = GET_MSHR_BANK(paddr);

  struct cache_action_t * prev = NULL;

  /* Check to see if there are any other requests to the same
     cache line; if so, coalesce the requests.  Note, this
     implementation of request-combining still uses one MSHR
     entry per request, but only one is sent to the next level
     of the cache hierarchy. */
  /* XXX: Disable combining for now  */
/*  for(int i=0;i<cp->MSHR_size;i++)
  {
    if((cp->MSHR[bank][i].cb != NULL) && 
        (cp->MSHR[bank][i].paddr >> cp->addr_shift) == (paddr >> cp->addr_shift) &&
        (cp->MSHR[bank][i].MSHR_link == NULL) &&
        (cp->MSHR[bank][i].when_returned == TICK_T_MAX))
    {
      cp->stat.MSHR_combos++;
      prev = &cp->MSHR[bank][i];
      break;
    }
  }
*/
  for(int i=0;i<cp->MSHR_size;i++)
    if(cp->MSHR[bank][i].cb == NULL)
    {
      cp->check_for_work = true;
      cp->check_for_MSHR_work = true;
      cp->check_for_MSHR_WB_work = true;
      cache_assert(cp->MSHR[bank][i].MSHR_link == NULL,NULL);
      cp->MSHR[bank][i].MSHR_link = NULL;
      if(prev)
      {
        /* if this request can be combined with others,
           then link it onto the end of the chain */
        prev->MSHR_link = &cp->MSHR[bank][i];
        cp->MSHR[bank][i].when_returned = prev->when_returned;
        cp->MSHR[bank][i].MSHR_linked = true;
      }
      else
      {
        cp->MSHR[bank][i].MSHR_linked = false;
      }
      return &cp->MSHR[bank][i];
    }

  fatal("request for MSHR failed");
}

static void MSHR_deallocate(
    const struct cache_t * const cp,
    const md_paddr_t paddr,
    const int index)
{
    const int bank = GET_MSHR_BANK(paddr);
    struct cache_action_t * MSHR = &cp->MSHR[bank][index];

    cache_assert(MSHR->cb, (void)0);
    MSHR->cb = NULL;

    if(MSHR->type == MSHR_WRITEBACK)
    {
      cp->MSHR_WB_num[bank]--;
      cache_assert(cp->MSHR_WB_num[bank] >= 0,(void)0);
    }
    else
    {
      cp->MSHR_num[bank]--;
      cache_assert(cp->MSHR_num[bank] >= 0,(void)0);
    }

    if(MSHR->cmd == CACHE_PREFETCH)
    {
      cp->MSHR_num_pf[bank]--;
      cache_assert(cp->MSHR_num_pf[bank] >= 0,(void)0);
    }

    if(MSHR->type == MSHR_MISS)
    {
      cp->MSHR_fill_num[bank]--;
      cache_assert(cp->MSHR_fill_num[bank] >= 0,(void)0);
    }
}

/* Insert a writeback request into the MSHR; assumes you already called MSHR_WB_available
   to make sure there's room. */
static void MSHR_WB_insert(
    struct cache_t * const cp,
    struct cache_line_t * const cache_block)
{
  md_paddr_t new_addr = cache_block->tag << cp->addr_shift;
  struct cache_action_t * MSHR = MSHR_allocate(cp, new_addr, CACHE_WRITEBACK);
  bool MSHR_linked = MSHR->MSHR_linked;
  int new_bank = GET_MSHR_BANK(new_addr);
  int new_index = MSHR - cp->MSHR[new_bank];

  cp->MSHR_WB_num[new_bank]++;
  cache_assert(cp->MSHR_WB_num[new_bank] <= cp->MSHR_WB_size, (void)NULL);

  MSHR->core = cache_block->core;
  MSHR->op = NULL;
  MSHR->PC = 0;
  MSHR->paddr = new_addr;
  MSHR->type = MSHR_WRITEBACK;
  MSHR->MSHR_bank = new_bank;
  MSHR->MSHR_index = new_index;
  MSHR->miss_cb_invoked = false;
  MSHR->cmd = CACHE_WRITEBACK;
  MSHR->prev_cp = cp;
  MSHR->cb = dummy_callback;
  MSHR->miss_cb = NULL;
  MSHR->translated_cb = NULL;
  MSHR->get_action_id = NULL;
  MSHR->when_enqueued = cache_get_cycle(cp);
  MSHR->when_started = TICK_T_MAX;
  MSHR->when_returned = TICK_T_MAX;
  MSHR->pipe_exit_time = TICK_T_MAX;
  MSHR->MSHR_linked = MSHR_linked;
  if(MSHR_linked)
    MSHR->when_started = cache_get_cycle(cp);

  cp->check_for_work = true;
  cp->check_for_MSHR_WB_work = true;
}

/* Insert a regular miss request into the MSHR; assumes you already called MSHR_available
   to make sure there's room. */
static void MSHR_insert(
    struct cache_t * const cp,
    struct cache_action_t * const ca)
{
    struct cache_action_t * MSHR = MSHR_allocate(cp,ca->paddr,ca->cmd);
    bool MSHR_linked = MSHR->MSHR_linked;
    *MSHR = *ca;
    MSHR->type = MSHR_MISS;
    MSHR->when_enqueued = cache_get_cycle(cp);
    MSHR->when_started = TICK_T_MAX;
    MSHR->when_returned = TICK_T_MAX;
    MSHR->core = ca->core;
    MSHR->MSHR_linked = MSHR_linked;
    int this_bank = GET_MSHR_BANK(ca->paddr);

    cp->MSHR_num[this_bank]++;
    cache_assert(cp->MSHR_num[this_bank] <= (cp->MSHR_size-cp->MSHR_WB_size), (void)NULL);

    if(MSHR_linked)
      MSHR->when_started = cache_get_cycle(cp);
    else
    {
      cp->MSHR_unprocessed_num[this_bank]++;
      cache_assert(cp->MSHR_unprocessed_num[this_bank] <= cp->MSHR_size,(void)0);
    }
    cp->MSHR_fill_num[this_bank]++;
    cache_assert(cp->MSHR_fill_num[this_bank] <= cp->MSHR_size,(void)0);

    if(ca->cmd == CACHE_PREFETCH)
    {
      cp->MSHR_num_pf[MSHR->MSHR_bank]++;
      cache_assert(cp->MSHR_num_pf[MSHR->MSHR_bank] <= cp->MSHR_size,(void)0);
    }
}

/* Called when a cache miss gets serviced.  May be recursively called for
   other (higher) cache hierarchy levels. */
void fill_arrived(
    struct cache_t * const cp,
    const int MSHR_bank,
    const int MSHR_index,
    const tick_t delay)
{
  cache_assert(MSHR_index >= 0 && MSHR_index < cp->MSHR_size, (void)0);
  struct cache_action_t * MSHR = &cp->MSHR[MSHR_bank][MSHR_index];
  cache_assert(!MSHR->MSHR_linked,(void)0);

  cp->check_for_work = true;
  cp->check_for_MSHR_fill_work = true;
  cp->check_for_MSHR_WB_work = true;

  if(MSHR->cb != NULL) /* original request was squashed */
    MSHR->when_returned = cache_get_cycle(cp) + delay;

  /* deal with combined/coalesced entries */
  struct cache_action_t * p = MSHR->MSHR_link, * next = NULL;
  while(p)
  {
    if((p->cb != NULL) && (p->when_returned == TICK_T_MAX))
    {
      cache_assert(p->MSHR_linked,(void)0);
      p->when_returned = cache_get_cycle(cp) + delay;
    }
    p->MSHR_linked = false;
    next = p->MSHR_link;
    p->MSHR_link = NULL;
    p = next;
  }
  MSHR->MSHR_link = NULL;

  /* NOTE: recursive calls to fill_arrived will be called
     by the MSHR_fill handling code in cache_process */
}

void print_heap(const struct cache_t * const cp)
{
  if(cp == uncore->LLC)
    return;
  for(int i=0;i<cp->banks;i++)
  {
    fprintf(stderr,"%s[%d] <%d>: {",cp->name,i,cp->pipe_num[i]);
    for(int j=1;j<=cp->pipe_num[i];j++)
      fprintf(stderr," %" PRId64"",cp->pipe[i][j].pipe_exit_time);
    fprintf(stderr," }\n");
  }
}


/* input state: assumes that the new node has just been inserted at
   insert_position and all remaining nodes obey the heap property */
static void fill_heap_balance(
    struct cache_fill_t * const pipe,
    const int insert_position)
{
  int pos = insert_position;
  //struct cache_fill_t tmp;
  while(pos > 1)
  {
    int parent = pos >> 1;
    if(pipe[parent].pipe_exit_time > pipe[pos].pipe_exit_time)
    {
      /* tmp = pipe[parent];
      pipe[parent] = pipe[pos];
      pipe[pos] = tmp; */
      memswap(&pipe[parent],&pipe[pos],sizeof(*pipe));
      pos = parent;
    }
    else
      return;
  }
}

/* input state: pipe_num is the number of elements in the heap
   prior to removing the root node. */
static void fill_heap_remove(
    struct cache_fill_t * const pipe,
    const int pipe_num)
{
  if(pipe_num == 1) /* only one node to remove */
  {
    pipe[1].valid = false;
    pipe[1].pipe_exit_time = TICK_T_MAX;
    return;
  }

  pipe[1] = pipe[pipe_num]; /* move last node to root */
  //struct cache_fill_t tmp;

  /* delete previous last node */
  pipe[pipe_num].valid = false;
  pipe[pipe_num].pipe_exit_time = TICK_T_MAX;

  /* push node down until heap property re-established */
  int pos = 1;
  while(1)
  {
    int Lindex = pos<<1;     // index of left child
    int Rindex = (pos<<1)+1; // index of right child
    tick_t myValue = pipe[pos].pipe_exit_time;

    tick_t Lvalue = TICK_T_MAX;
    tick_t Rvalue = TICK_T_MAX;
    if(Lindex < pipe_num) /* valid index */
      Lvalue = pipe[Lindex].pipe_exit_time;
    if(Rindex < pipe_num) /* valid index */
      Rvalue = pipe[Rindex].pipe_exit_time;

    if(((myValue > Lvalue) && (Lvalue != TICK_T_MAX)) ||
       ((myValue > Rvalue) && (Rvalue != TICK_T_MAX)))
    {
      if(Rvalue == TICK_T_MAX) /* swap pos with L */
      {
        /* tmp = pipe[Lindex];
        pipe[Lindex] = pipe[pos];
        pipe[pos] = tmp; */
        memswap(&pipe[Lindex],&pipe[pos],sizeof(*pipe));
        return;
      }
      else if((myValue > Lvalue) && (Lvalue < Rvalue))
      {
        /* tmp = pipe[Lindex];
        pipe[Lindex] = pipe[pos];
        pipe[pos] = tmp; */
        memswap(&pipe[Lindex],&pipe[pos],sizeof(*pipe));
        pos = Lindex;
      }
      else /* swap pos with R */
      {
        /* tmp = pipe[Rindex];
        pipe[Rindex] = pipe[pos];
        pipe[pos] = tmp; */
        memswap(&pipe[Rindex],&pipe[pos],sizeof(*pipe));
        pos = Rindex;
      }
    }
    else
      return;
  }
}
/* We are assuming separate ports for cache fills; returns true if the fill pipeline
   can accept a fill request. */
static inline bool cache_fillable(
    const struct cache_t * const cp,
    const md_paddr_t paddr)
{
  const int bank = GET_BANK(paddr);
  if(cp->fill_num[bank] < cp->latency)
    return true;
  else
    return false;
}

/* Do the actual fill; assume you already called cache_fillable */
static inline void cache_fill(
    struct cache_t * const cp,
    const enum cache_command cmd,
    const md_paddr_t paddr,
    struct core_t * core)
{
  const int bank = GET_BANK(paddr);
  const int insert_position = cp->fill_num[bank]+1;
  cache_assert(!cp->fill_pipe[bank][insert_position].valid,(void)0);

  cp->fill_pipe[bank][insert_position].valid = true;
  cp->fill_pipe[bank][insert_position].cmd = cmd;
  cp->fill_pipe[bank][insert_position].paddr = paddr;
  cp->fill_pipe[bank][insert_position].core = core;

  cp->fill_pipe[bank][insert_position].pipe_exit_time = cache_get_cycle(cp)+cp->latency;

  assert(insert_position < cp->heap_size);
  fill_heap_balance(cp->fill_pipe[bank],insert_position);

  cp->fill_num[bank]++;
  cache_assert(cp->fill_num[bank] <= cp->latency,(void)0);
  cp->check_for_work = true;
  cp->check_for_fill_work = true;
}

/* update hit/miss stats for a request about to leave the pipeline. */
static void update_request_stats(
    struct cache_t * const cp,
    struct cache_action_t * const ca,
    struct cache_line_t * const line,
    const bool hit)
{
  /* LLC stats are per core */
  if((cp == uncore->LLC) && (ca->core)) {
    CACHE_STAT(cp->stat.core_lookups[ca->core->id]++;)
    if(!hit)
      CACHE_STAT(cp->stat.core_misses[ca->core->id]++;)
  }

  /* Per-request type stats */
  switch(ca->cmd)
  {
    case CACHE_READ:
      CACHE_STAT(cp->stat.load_lookups++;)
      if(!hit)
        CACHE_STAT(cp->stat.load_misses++;)
      break;
    case CACHE_PREFETCH:
      CACHE_STAT(cp->stat.prefetch_lookups++;)
      if(!hit)
        CACHE_STAT(cp->stat.prefetch_misses++;)
      break;
    case CACHE_WRITE:
      CACHE_STAT(cp->stat.store_lookups++;)
      if(!hit)
        CACHE_STAT(cp->stat.store_misses++;)
      break;
    case CACHE_WRITEBACK:
      CACHE_STAT(cp->stat.writeback_lookups++;)
      if(!hit)
        CACHE_STAT(cp->stat.writeback_misses++;)
      break;
    default:
      break;
  }

  /* Mark the line as prefetched */
  if(hit && line && ca->cmd != CACHE_PREFETCH &&
     line->prefetched && !line->prefetch_used)
  {
    line->prefetch_used = true;
    CACHE_STAT(cp->stat.prefetch_useful_insertions++;)
  }
}

/* simulate one cycle of sending MSHR writeback requests */
static void cache_process_MSHR_WB(struct cache_t * const cp, int start_point)
{
  int b;
  bool MSHR_WB_work_found = false;
  cache_assert(cp->controller, (void)0);
  if(cp->check_for_MSHR_WB_work)
  {
    /* Check if we can use bus to upper level */
    if(cp->controller->can_schedule_upstream())
    {
      /* process write-backs in MSHR (in FIFO order) */
      for(b=0;b<cp->MSHR_banks;b++)
      {
        int bank = (start_point+b) & cp->MSHR_mask;
        if(cp->MSHR_WB_num[bank]) /* there are cast-outs pending to be written back */
        {
          struct cache_action_t * MSHR_WB;
          tick_t oldest_time = TICK_T_MAX;
          int oldest_index = -1;
          for(int j=0; j < cp->MSHR_size; j++)
          {
            MSHR_WB = &cp->MSHR[bank][j];
            cache_assert(MSHR_WB, (void)0);
            if(MSHR_WB->cb == NULL)
              continue;

            if(MSHR_WB->type != MSHR_WRITEBACK)
              continue;

            if(MSHR_WB->when_started != TICK_T_MAX)
              continue;

            if(MSHR_WB->when_enqueued < oldest_time)
            {
              oldest_time = MSHR_WB->when_enqueued;
              oldest_index = j;
            }
          }

          if(oldest_index < 0)
              continue;

          MSHR_WB = &cp->MSHR[bank][oldest_index];
          cache_assert(MSHR_WB, (void)0);

          MSHR_WB_work_found = true;
          /* Let controller handle sending a request to upper level */
          cp->controller->send_request_upstream(bank, oldest_index, MSHR_WB);
        }
      }
    }
    else
      MSHR_WB_work_found = cp->check_for_MSHR_WB_work;

    /* Drop fullfilled WB requests */
    for(b=0;b<cp->MSHR_banks;b++)
    {
      int bank = (start_point+b) & cp->MSHR_mask;
      struct cache_action_t * MSHR_WB;
      for(int j=0; j < cp->MSHR_size; j++)
      {
        MSHR_WB = &cp->MSHR[bank][j];
        cache_assert(MSHR_WB, (void)0);
        if (MSHR_WB->cb == NULL)
          continue;
        if (MSHR_WB->type != MSHR_WRITEBACK)
          continue;
        if (MSHR_WB->when_started == TICK_T_MAX)
          continue;
        MSHR_WB_work_found = true;
        if (MSHR_WB->when_returned <= cache_get_cycle(cp))
          MSHR_deallocate(cp, MSHR_WB->paddr, j);
      }
    }
  }
  cp->check_for_MSHR_WB_work = MSHR_WB_work_found;
}

/* simulate one cycle of scheduling returned MSHR requests to fill pipe */
static void cache_process_MSHR_fill(struct cache_t * const cp, int start_point)
{
  int b;
  bool MSHR_fill_work_found = false;

  if(cp->check_for_MSHR_fill_work)
    for(b=0;b<cp->MSHR_banks;b++)
    {
      int bank = (start_point + b) & cp->MSHR_mask;
      if(cp->MSHR_fill_num[bank])
      {
        MSHR_fill_work_found = true;
        tick_t oldest = TICK_T_MAX;
        int old_index = -1;

        /* find oldest returned request to process */
        for(int i=0;i<cp->MSHR_size;i++)
        {
          struct cache_action_t * MSHR = &cp->MSHR[bank][i];
          if(MSHR->type == MSHR_WRITEBACK)
            continue;

          if(MSHR->cb && (MSHR->when_returned <= cache_get_cycle(cp)) &&
                  cache_fillable(cp,MSHR->paddr))
          {
            if(MSHR->when_enqueued < oldest)
            {
              oldest = MSHR->when_enqueued;
              old_index = i;
            }
          }
        }

        if(old_index < 0)
          continue;

        /* process returned MSHR requests that now need to fill the cache */
        struct cache_action_t * MSHR = &cp->MSHR[bank][old_index];

        if(MSHR->cb && (MSHR->when_returned <= cache_get_cycle(cp)) &&
           cp->controller->can_schedule_downstream(MSHR->prev_cp))
        {
          if(!MSHR->MSHR_linked)
          {
            int insert = true;
            if(cp->PF_filter && (MSHR->cmd == CACHE_PREFETCH))
              insert = prefetch_filter_lookup(cp, cp->PF_filter,(MSHR->paddr>>cp->addr_shift));

            /* if using prefetch buffer, insert there instead */
            if((MSHR->cmd == CACHE_PREFETCH) && (cp->PF_buffer) && insert)
            {
              struct prefetch_buffer_t *p, *evictee = NULL, *prev = NULL;
              p = cp->PF_buffer;
              while(p)
              {
                if((MSHR->paddr>>cp->addr_shift) == (p->addr>>cp->addr_shift)) /* already been prefetched */
                  break;
                if(!p->next || (p->addr == (md_paddr_t)-1))
                {
                  evictee = p;
                  break;
                }
                prev = p;
                p = p->next;
              }

              if(evictee) /* not found, overwrite entry */
                p->addr = MSHR->paddr;

              /* else someone else already prefetched this addr */
              /* move to MRU position if not already there */
              if(prev)
              {
                prev->next = p->next;
                p->next = cp->PF_buffer;
                cp->PF_buffer = p;
              }
            }
            else if(  (MSHR->cmd == CACHE_READ) || (MSHR->cmd == CACHE_PREFETCH) ||
                     ((MSHR->cmd == CACHE_WRITE || MSHR->cmd == CACHE_WRITEBACK) && (cp->allocate_policy == WRITE_ALLOC)))
            {
              if(insert)
                cache_fill(cp,MSHR->cmd,MSHR->paddr,MSHR->core);
            }
          }

          /* If this is not an L1 cache (prev_cp != NULL), do not invoke callback; only
             invoke it when the request finally makes its way back down to the L1 */
          if(!MSHR->prev_cp && MSHR->cb && MSHR->op && (MSHR->action_id == MSHR->get_action_id(MSHR->op)))
            MSHR->cb(MSHR->op);

          cp->controller->send_response_downstream(MSHR);

          MSHR_deallocate(cp, MSHR->paddr, old_index);
        }
      }
    }
  cp->check_for_MSHR_fill_work = MSHR_fill_work_found;
}

/* simulate one cycle of fills to the array */
static void cache_process_fills(struct cache_t * const cp, int start_point)
{
  int b;
  bool fill_work_found = false;

  /* process cache fills: */
  if(cp->check_for_fill_work)
    for(b=0;b<cp->banks;b++)
    {
      int bank = (start_point + b) & cp->bank_mask;

      struct cache_fill_t * cf = &cp->fill_pipe[bank][1];
      if(cp->fill_num[bank])
      {
        fill_work_found = true;
        if(cf->valid && cf->pipe_exit_time <= cache_get_cycle(cp))
        {
          if(!cache_peek(cp,cf->paddr)) /* make sure hasn't been filled in the meantime */
          {
            struct cache_line_t * p = cache_get_evictee(cp,cf->paddr,cf->core);
            md_paddr_t new_addr = p->tag << cp->addr_shift;
            if(p->valid)
            {
              if(p->dirty)
              {
                if(cp->write_policy == WRITE_BACK)
                {
                  if(!MSHR_WB_available(cp, new_addr))
                    goto no_MSHR_available;

                  MSHR_WB_insert(cp, p);
                }
              }
              else /* not dirty, insert in WBB just as victim */
              {
                //MSHR_WB_victim_insert(cp,p);
              }
            }

            if(p->prefetched && cp->PF_filter)
            {
              prefetch_filter_update(cp, cp->PF_filter,p->tag,p->prefetch_used);
            }
            p->valid = p->dirty = false; /* this removes the copy in the cache since the WBB has it now */

            cache_insert_block(cp,cf->cmd,cf->paddr,cf->core);
          }
          fill_heap_remove(cp->fill_pipe[bank],cp->fill_num[bank]);
          cp->fill_num[bank]--;
          cache_assert(cp->fill_num[bank] >= 0,(void)0);
        }
      }
    no_MSHR_available:
      ;
    }
  cp->check_for_fill_work = fill_work_found;
}

/* simulate one cycle of the cache pipeline */
static void cache_process_pipe(struct cache_t * const cp, int start_point)
{
  int b;
  bool pipe_work_found = false;

  /* check last stage of cache pipes, process accesses */
  if(cp->check_for_pipe_work)
    for(b=0;b<cp->banks;b++)
    {
      int bank = (start_point + b) & cp->bank_mask;

      struct cache_action_t * ca = &cp->pipe[bank][1]; // heap root
      if(cp->pipe_num[bank])
      {
        /* These two determine whether to check the prefetcher(s) for
           new address(es) to prefetch.  We only do so when the request
           leaves the cache-lookup pipeline (request dequeued) to prevent
           double-prefetching */
        bool do_prefetch = !cp->prefetch_on_miss;

        pipe_work_found = true;
        if(ca->cb)
        {
          if(ca->pipe_exit_time <= cache_get_cycle(cp))
          {
            /* Check if request isn't already in an MSHR */
            int this_bank = GET_MSHR_BANK(ca->paddr);
            int MSHR_index = -1;
            for(int j=0; j<cp->MSHR_size; j++)
            {
              struct cache_action_t * MSHR = &cp->MSHR[this_bank][j];
              cache_assert(MSHR, (void)0);
              if(MSHR->cb && (MSHR->paddr == ca->paddr))
              {
                MSHR_index = j;
                break;
              }
            }

            if(MSHR_index >= 0)
            {
              struct cache_action_t * MSHR = &cp->MSHR[this_bank][MSHR_index];
              controller_response_t res = cp->controller->check_MSHR(MSHR);
              /* Waiting on a coherence result / result from a higher level */
              if(res == MSHR_STALL)
                continue;
            }

            /* Check cache array and ask controller for an OK after hit there */
            struct cache_line_t * line = cache_is_hit(cp,ca->cmd,ca->paddr,ca->core);

            controller_array_response_t res = cp->controller->check_array(line);
            if(res == ARRAY_MISS)
              line = NULL;

            if(line != NULL) /* if cache hit */
            {
              bool needs_WB = (ca->cmd == CACHE_WRITE || ca->cmd == CACHE_WRITEBACK) && (cp->write_policy == WRITE_THROUGH);
              if(needs_WB && !MSHR_WB_available(cp,ca->paddr))
                continue; /* can't go until MSHR is available */

              if(!cp->controller->can_schedule_downstream(ca->prev_cp))
                continue; /* or until controller says we can respond downstream */

              /* on a write-through cache, use a MSHR entry to send write to the next level */
              if(needs_WB)
              {
                struct cache_line_t tmp_line;
                tmp_line.core = ca->core;
                tmp_line.tag = ca->paddr >> cp->addr_shift;
                tmp_line.valid = true;
                tmp_line.dirty = false;

                MSHR_WB_insert(cp,&tmp_line);
              }

              /* For L1 caches only: (L2/LLC handled by MSHR processing later)
                 invoke the call-back function; the action-id check makes sure the request is
                 still valid (e.g., a request is invalid if the original uop that initiated
                 the request had been flushed due to a branch misprediction). */
              if(!ca->prev_cp && ca->op && (ca->action_id == ca->get_action_id(ca->op)))
              {
#ifdef ZTRACE
                CACHE_ZTRACE("%s|hit",cp->name);
#endif
                ca->cb(ca->op);
              }

              update_request_stats(cp, ca, line, true);

              /* fill previous level as appropriate */
              cp->controller->send_response_downstream(ca);
            }
            else /* miss in main data array */
            {
              int last_chance_hit = false;

              if((ca->cmd == CACHE_READ) ||
                 (ca->cmd == CACHE_PREFETCH) ||
                 ((ca->cmd == CACHE_WRITE || ca->cmd == CACHE_WRITEBACK) && cp->write_combining))
              {
                /* last chance: check in WBBs - this implements something like a victim cache */
                /* no WBBs now, just check prefetech buffer */
                if(cp->PF_buffer && !last_chance_hit)
                {
                  struct prefetch_buffer_t * p = cp->PF_buffer, * prev = NULL;
                  while(p)
                  {
                    if((p->addr>>cp->addr_shift) == (ca->paddr>>cp->addr_shift))
                    {
                      last_chance_hit = true;
                      break;
                    }
                    prev = p;
                    p = p->next;
                  }

                  if(p) /* hit in prefetch buffer */
                  {
                    last_chance_hit = true;

                    /* insert block into cache */
                    struct cache_line_t * evictee = cache_get_evictee(cp,ca->paddr,ca->core);
                    int ok_to_insert = true;
                    if((cp->write_policy == WRITE_BACK) && evictee->valid && evictee->dirty)
                    {
                      if(MSHR_WB_available(cp,ca->paddr))
                      {
                        MSHR_WB_insert(cp,evictee);
                      }
                      else /* we'd like to insert the prefetched line into the cache, but
                              in order to do so, we need to writeback the evictee but
                              we can't get a MSHR.  */
                      {
                        ok_to_insert = false;
                      }
                    }

                    if(ok_to_insert && cache_fillable(cp,ca->paddr))
                    {
                      evictee->valid = evictee->dirty = false;
                      cache_fill(cp,CACHE_PREFETCH,p->addr,ca->core);
                    }
                    else /* if we couldn't do the insertion for some reason (e.g., no WBB
                            entry for dirty evictee or now fill bandwidth, move the
                            block back to the MRU position in the PF-buffer and hope
                            for better luck next time. */
                    {
                      if(prev)
                      {
                        prev->next = p->next;
                        p->next = cp->PF_buffer;
                        cp->PF_buffer = p;
                      }
                    }
                  }
                }
              }

              if(last_chance_hit)
              {
                // FIXME: This whole last_chance section needs to be reowrked!
                cache_assert(cp->controller->can_schedule_downstream(ca->prev_cp), (void)0);

                /* only invoke callback if this is an L1 cache (see earlier comment
                   for a regular cache hit. */
                if(!ca->prev_cp && ca->op && (ca->action_id == ca->get_action_id(ca->op)))
                  ca->cb(ca->op);

                update_request_stats(cp, ca, NULL, true);

                /* fill previous level as appropriate */
                cp->controller->send_response_downstream(ca);
              }
              else /* ok, we really missed */
              {
                /* place in MSHR */
                do_prefetch = true;

                if(!MSHR_available(cp,ca->paddr))
                  /* this circumvents the "ca->cb = NULL" at the end of the loop, thereby
                   * leaving the ca in the pipe */
                  continue;

                MSHR_insert(cp, ca);

                /* update miss stats when requests *leaves* the cache pipeline to avoid
                 * double-counting */
                update_request_stats(cp, ca, NULL, false);
              }
            }

            if(do_prefetch) {
              if(ca->PC && (ca->cmd == CACHE_READ)) {
                for(int ii=0;ii<cp->num_prefetchers;ii++) {
                  md_paddr_t pf_addr;
                  if(!ca->prefetcher_hint)
                    pf_addr = cp->prefetcher[ii]->lookup(ca->PC,ca->paddr);
                  else
                    pf_addr = cp->prefetcher[ii]->latest_lookup(ca->PC,ca->paddr);

                  if(memory::page_round_down(pf_addr)) { /* don't prefetch from zeroth page */
                    int j;

                    /* search PFF to see if pf_addr already requested */
                    int already_requested = false;
                    int index = cp->PFF_head;;
                    for(j=0;j<cp->PFF_num;j++)
                    {
                      if(cp->PFF[index].addr == pf_addr)
                      {
                        already_requested = true;
                        break;
                      }
                      index = modinc(index,cp->PFF_size);
                    }
                    if(already_requested)
                      continue; /* for(i=0;...) */

                    /* if FIFO full, overwrite oldest */
                    if(cp->PFF_num == cp->PFF_size) {
                      cp->PFF_head = modinc(cp->PFF_head,cp->PFF_size); //(cp->PFF_head + 1) % cp->PFF_size;
                      cp->PFF_num--;
                      cache_assert(cp->PFF_num >= 0,(void)0);
                    }

                    cp->PFF[cp->PFF_tail].PC = ca->PC;
                    cp->PFF[cp->PFF_tail].core = ca->core;
                    cp->PFF[cp->PFF_tail].addr = pf_addr;
                    cp->PFF_tail = modinc(cp->PFF_tail,cp->PFF_size); //(cp->PFF_tail + 1) % cp->PFF_size;
                    cp->PFF_num++;
                    cache_assert(cp->PFF_num <= cp->PFF_size,(void)0);
                  }
                }
              }
            }
            cache_heap_remove(cp->pipe[bank],cp->pipe_num[bank]);
            cp->pipe_num[bank]--;
            cache_assert(cp->pipe_num[bank] >= 0,(void)0);
          }
        }
        else /* !ca->cb */
        {
          cache_heap_remove(cp->pipe[bank],cp->pipe_num[bank]);
          cp->pipe_num[bank]--;
          cache_assert(cp->pipe_num[bank] >= 0,(void)0);
        }
      }
   }
  cp->check_for_pipe_work = pipe_work_found;
}

/* simulate one cycle of MSHR requests */
static void cache_process_MSHR(struct cache_t * const cp, int start_point)
{
  int b;
  bool MSHR_work_found = false;

  if(cp->check_for_MSHR_work)
  {
    if(cp->controller->can_schedule_upstream())
    {
      if(cp->MSHR_cmd_order == NULL)
      {
        /* First-come First-serve processing of MSHR requests. */
        for(b=0;b<cp->MSHR_banks;b++)
        {
          int bank = (start_point+b) & cp->MSHR_mask;
          if(cp->MSHR_num[bank])
          {
            MSHR_work_found = true;
            /* find oldest not-processed entry */
            if(cp->MSHR_unprocessed_num[bank] > 0)
            {
              tick_t oldest_age = TICK_T_MAX;
              int index = -1;
              for(int i=0;i<cp->MSHR_size;i++)
              {
                struct cache_action_t * MSHR = &cp->MSHR[bank][i];
                if(MSHR->type == MSHR_WRITEBACK)
                  continue;
                if(MSHR->cb && (MSHR->when_started == TICK_T_MAX) && (MSHR->when_enqueued < oldest_age) && (!MSHR->translated_cb || MSHR->translated_cb(MSHR->op,MSHR->action_id))) /* if DL1, don't go until translated */
                {
                  oldest_age = MSHR->when_enqueued;
                  index = i;
                }

                /* If this operation is waiting on a TLB miss, it could take awhile. Invoke the miss
                   callback function to reduce replays. */
                if(!MSHR->miss_cb_invoked && (MSHR->translated_cb && !MSHR->translated_cb(MSHR->op,MSHR->action_id)))
                {
                  if(MSHR->miss_cb && MSHR->op && (MSHR->action_id == MSHR->get_action_id(MSHR->op)))
                  {
#ifdef ZTRACE
                    CACHE_ZTRACE("%s|miss",cp->name);
#endif
                    MSHR->miss_cb_invoked = true;
                    MSHR->miss_cb(MSHR->op,BIG_LATENCY);
                  }
                }
              }

              if(index < 0)
                continue;

              struct cache_action_t * MSHR = &cp->MSHR[bank][index];
              if(MSHR->cb)
              {
                if(MSHR->MSHR_linked)
                {
                  /* this entry combined/coalesced, but still need to invoke miss_cb */
                  MSHR->when_started = cache_get_cycle(cp);
                  cp->MSHR_unprocessed_num[bank]--;
                  cache_assert(cp->MSHR_unprocessed_num[bank] >= 0,(void)0);
                  if(MSHR->miss_cb && MSHR->op && (MSHR->action_id == MSHR->get_action_id(MSHR->op)))
                    MSHR->miss_cb(MSHR->op,cp->next_level->latency); /* restarts speculative scheduling */
                  break;
                }
                else
                {
                  /* let controller handle next level request */
                  if(cp->controller->send_request_upstream(bank, index, MSHR))
                  {
                    cp->MSHR_unprocessed_num[bank]--;
                    cache_assert(cp->MSHR_unprocessed_num[bank] >= 0,(void)0);
                    /* Invoke miss callback to let core know about new expected latency */
                    if(MSHR->miss_cb && MSHR->op &&
                       (MSHR->action_id == MSHR->get_action_id(MSHR->op)))
                    {
                      int miss_latency;
                      if(cp->next_level)
                        miss_latency = cp->next_level->latency;
                      else
                      {
                        miss_latency = BIG_LATENCY;
#ifdef ZTRACE
                        CACHE_ZTRACE("%s|miss",cp->name);
#endif
                      }
                      MSHR->miss_cb(MSHR->op, miss_latency); /* restarts speculative scheduling */
                    }
                    break;
                  }
                }
              }
            }
          }
        }
      }
      else
      {
        /* Prioritized handling of MSHR requests by type (read, write, prefetch, writeback) */
        int sent_something = false;
        int c;

        for(c=0;c<4 && !sent_something;c++)
        {
          enum cache_command cmd = cp->MSHR_cmd_order[c];

          for(b=0;b<cp->MSHR_banks;b++)
          {
            int bank = (start_point+b) & cp->MSHR_mask;
            if(cp->MSHR_num[bank])
            {
              MSHR_work_found = true;
              /* find oldest not-processed entry */
              if(cp->MSHR_unprocessed_num[bank] > 0)
              {
                tick_t oldest_age = TICK_T_MAX;
                int index = -1;

                for(int i=0;i<cp->MSHR_size;i++)
                {
                  struct cache_action_t * MSHR = &cp->MSHR[bank][i];
                  if(MSHR->type == MSHR_WRITEBACK)
                    continue;
                  if((MSHR->cmd == cmd) && MSHR->cb && (MSHR->when_started == TICK_T_MAX) && (!MSHR->translated_cb || MSHR->translated_cb(MSHR->op,MSHR->action_id))) /* if DL1, don't go until translated */
                  {
                    if(MSHR->when_enqueued < oldest_age)
                    {
                      oldest_age = MSHR->when_enqueued;
                      index = i;
                    }
                  }
                }

                if(index < 0)
                  continue;

                struct cache_action_t * MSHR = &cp->MSHR[bank][index];


                /* If this operation is waiting on a TLB miss, it could take awhile. Invoke the miss
                   callback function to reduce replays. */
                if(!MSHR->miss_cb_invoked && (MSHR->translated_cb && !MSHR->translated_cb(MSHR->op,MSHR->action_id)))
                {
                  cache_assert(MSHR->get_action_id, (void)0);
                  if(MSHR->miss_cb && MSHR->op && (MSHR->action_id == MSHR->get_action_id(MSHR->op)))
                  {
#ifdef ZTRACE
                    CACHE_ZTRACE("%s|miss",cp->name);
#endif
                    MSHR->miss_cb_invoked = true;
                    MSHR->miss_cb(MSHR->op,BIG_LATENCY);
                  }
                }

                assert((MSHR->cmd == cmd) && MSHR->cb);

                if(MSHR->MSHR_linked)
                {
                  /* this entry combined/coalesced, but still need to invoke miss_cb */
                  MSHR->when_started = cache_get_cycle(cp);
                  cp->MSHR_unprocessed_num[bank]--;
                  cache_assert(cp->MSHR_unprocessed_num[bank] >= 0,(void)0);
                  if(MSHR->miss_cb && MSHR->op && (MSHR->action_id == MSHR->get_action_id(MSHR->op)))
                    MSHR->miss_cb(MSHR->op,cp->next_level->latency); /* restarts speculative scheduling */
                  sent_something = true;
                  break;
                }
                else
                {
                  /* let controller handle next level request */
                  if(cp->controller->send_request_upstream(bank, index, MSHR))
                  {
                    cp->MSHR_unprocessed_num[bank]--;
                    cache_assert(cp->MSHR_unprocessed_num[bank] >= 0,(void)0);
                    /* Invoke miss callback to let core know about new expected latency */
                    if(MSHR->miss_cb && MSHR->op &&
                       (MSHR->action_id == MSHR->get_action_id(MSHR->op)))
                    {
                      int miss_latency;
                      if(cp->next_level)
                        miss_latency = cp->next_level->latency;
                      else
                      {
                        miss_latency = BIG_LATENCY;
#ifdef ZTRACE
                        CACHE_ZTRACE("%s|miss",cp->name);
#endif
                      }
                      MSHR->miss_cb(MSHR->op, miss_latency); /* restarts speculative scheduling */
                    }
                    sent_something = true;
                    break;
                  }
                }
              }
            }
          }
        }
      }
    }
    else /* bus not available */
    {
      /* were there any outstanding requests anyway? */
      for(b=0;b<cp->MSHR_banks;b++)
      {
        int bank = (start_point+b) & cp->MSHR_mask;
        if(cp->MSHR_num[bank])
        {
          MSHR_work_found = true;
          break;
        }
      }
    }
  }
  cp->check_for_MSHR_work = MSHR_work_found;

  /* occupancy stats */
  int bank;
  int max_size = cp->MSHR_banks * cp->MSHR_size;
  int total_occ = 0;
  for(bank=0;bank<cp->MSHR_banks;bank++)
  {
    total_occ += cp->MSHR_num[bank] + cp->MSHR_WB_num[bank];
  }
  CACHE_STAT(cp->stat.MSHR_occupancy += total_occ;)
  CACHE_STAT(cp->stat.MSHR_full_cycles += (total_occ == max_size);)
}

/* simulate one cycle of the cache */
void cache_process(struct cache_t * const cp)
{
  /* Advance local cycle count */
  if (cp != uncore->LLC)
    cp->sim_cycle++;

  if (!cp->check_for_work)
    return;

  //int start_point = random() & cp->bank_mask; /* randomized arbitration */
  int start_point = cp->start_point; /* round-robin arbitration */
  cp->start_point = modinc(cp->start_point,cp->banks);

  cache_process_MSHR_WB(cp, start_point);
  cache_process_MSHR_fill(cp, start_point);
  cache_process_fills(cp, start_point);
  cache_process_pipe(cp, start_point);
  cache_process_MSHR(cp, start_point);

  cp->check_for_work = cp->check_for_MSHR_WB_work ||
                        cp->check_for_MSHR_fill_work ||
                        cp->check_for_fill_work ||
                        cp->check_for_pipe_work ||
                        cp->check_for_MSHR_work;
}

/* Attempt to enqueue a prefetch request, based on the predicted
   prefetch addresses in the prefetch FIFO (PFF) */
static void cache_prefetch(struct cache_t * const cp)
{
  /* if the PF controller says the bus hasn't been too busy */
  if(!cp->PF_sample_interval || (cp->PF_state == PF_OK))
  {
    /* check prefetch FIFO for new prefetch requests - max one per cycle */
    if(cp->PFF && cp->PFF_num)
    {
      md_paddr_t pf_addr = cp->PFF[cp->PFF_head].addr;
      struct core_t * core = cp->PFF[cp->PFF_head].core; // tracks originating/owner core
      const int bank = GET_MSHR_BANK(pf_addr);

      if((cp->MSHR_num[bank] < cp->prefetch_threshold) /* if MSHR is too full, don't add more requests */
         && (cp->MSHR_num_pf[bank] < cp->prefetch_max))
      {
        md_addr_t pf_PC = cp->PFF[cp->PFF_head].PC;
        if(cache_enqueuable(cp, memory::DO_NOT_TRANSLATE, pf_addr))
        {
          cache_enqueue(core, cp, NULL, CACHE_PREFETCH, memory::DO_NOT_TRANSLATE, pf_PC, pf_addr, (seq_t)-1, bank, NO_MSHR, NULL, dummy_callback, NULL, NULL, NULL);
          cp->PFF_head = modinc(cp->PFF_head,cp->PFF_size); //(cp->PFF_head+1) % cp->PFF_size;
          cp->PFF_num --;
          cache_assert(cp->PFF_num >= 0,(void)0);
        }
      }
    }
  }
}

/* Update the bus-utilization-based prefetch controller. */
static void prefetch_controller_update(struct cache_t * const cp)
{
  if(cp->PF_sample_interval && ((cache_get_cycle(cp) % cp->PF_sample_interval) == 0))
  {
    int current_sample = (int)cp->next_bus->stat.utilization;
    double duty_cycle = (current_sample - cp->PF_last_sample) / (double) cp->PF_sample_interval;
    if(duty_cycle < cp->PF_low_watermark)
      cp->PF_state = PF_OK;
    else if(duty_cycle > cp->PF_high_watermark)
      cp->PF_state = PF_REFRAIN;
    /* else stay in current state */
    cp->PF_last_sample = current_sample;
  }
}

void step_LLC_PF_controller(struct uncore_t * const uncore)
{
  /* update prefetch controllers */
  prefetch_controller_update(uncore->LLC);
}

void prefetch_LLC(struct uncore_t * const uncore)
{
  cache_prefetch(uncore->LLC);
}

void step_core_PF_controllers(struct core_t * const core)
{
  /* update prefetch controllers */
  if(core->memory.DL2) prefetch_controller_update(core->memory.DL1);
  prefetch_controller_update(core->memory.DL1);
  if(core->memory.IL1) prefetch_controller_update(core->memory.IL1);
}

void prefetch_core_caches(struct core_t * const core)
{
  if(core->memory.DL2) cache_prefetch(core->memory.DL1);
  cache_prefetch(core->memory.DL1);
  if(core->memory.IL1) cache_prefetch(core->memory.IL1);
}

void cache_freeze_stats(struct core_t * const core)
{
  core->memory.IL1->frozen = true;
  core->memory.DL1->frozen = true;
  if(core->memory.DL2) core->memory.DL2->frozen = true;
  core->memory.ITLB->frozen = true;
  core->memory.DTLB->frozen = true;
  if(core->memory.DTLB2) core->memory.DTLB2->frozen = true;
}

tick_t cache_get_cycle(const struct cache_t * const cp)
{
  if(cp == uncore->LLC)
    return uncore->sim_cycle;
  return cp->sim_cycle;
}

void cache_print(const struct cache_t * const cp)
{
  fprintf(stderr,"<<<<< %s >>>>>\n",cp->name);
  fprintf(stderr, "current cycle: %" PRId64"\n", cache_get_cycle(cp));
  for(int i=0;i<cp->banks;i++)
  {
    int j;
    fprintf(stderr,"bank[%d]: { ",i);
    for(j=0;j<cp->latency;j++)
    {
      if(cp->pipe[i][j].cb)
      {
        if(cp->pipe[i][j].cmd == CACHE_READ)
          fprintf(stderr,"L:");
        else if(cp->pipe[i][j].cmd == CACHE_WRITE || cp->pipe[i][j].cmd == CACHE_WRITEBACK)
          fprintf(stderr,"S:");
        else
          fprintf(stderr,"P:");
        fprintf(stderr,"%p(%" PRId64")",cp->pipe[i][j].op,((struct uop_t*)cp->pipe[i][j].op)->decode.uop_seq);
      }
      else
        fprintf(stderr,"---");

      if(j != (cp->latency-1))
        fprintf(stderr,", ");
    }

    fprintf(stderr,"fill: { ");
    for(j=0;j<cp->latency;j++)
    {
      if(cp->fill_pipe[i][j].valid)
        fprintf(stderr,"%" PRIx64"",cp->fill_pipe[i][j].paddr);
      else
        fprintf(stderr,"---");
      if(j != cp->latency-1)
        fprintf(stderr,", ");
    }
    fprintf(stderr," }\n");

    for(j=0;j<cp->MSHR_size;j++)
    {
      fprintf(stderr,"MSHR[%d]",j);
      if(cp->MSHR[i][j].cb)
      {
        fprintf(stderr," %c:",(cp->MSHR[i][j].cmd==CACHE_READ)?'L':'S');
        fprintf(stderr,"%p(%" PRId64")",cp->MSHR[i][j].op,((struct uop_t*)cp->MSHR[i][j].op)->decode.uop_seq);
      }
      fprintf(stderr,"\n");
    }
    fprintf(stderr," }\n");
  }
}
