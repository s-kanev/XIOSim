/* zesto-dram.cpp - Zesto main memory/DRAM class
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
#include <cstring>
#include <limits.h>

#include "misc.h"
#include "stats.h"

#include "zesto-cache.h"
#include "zesto-dram.h"
#include "zesto-uncore.h"

/* Many of the DRAM variables are global and not replicated on a per-core
   basis since we assume a single memory interface for the entire multi-core
   chip. */

/* this pointer hold DRAM implementation/model-specific state */
std::unique_ptr<class dram_t> dram;

void dram_t::init(void)
{
  total_accesses = 0;
  total_latency = 0;
  best_latency = 0;
  worst_latency = 0;
  total_burst = 0;
}

dram_t::dram_t(void)
{
  init();
}

dram_t::~dram_t()
{
}

void dram_t::refresh(void)
{
}

void dram_t::reg_stats(xiosim::stats::StatsDatabase* sdb)
{
    auto& total_access_st =
            stat_reg_counter(sdb, true, "dram.total_access", "total number of memory accesses",
                             &total_accesses, 0, TRUE, NULL);
    auto& total_latency_st =
            stat_reg_counter(sdb, false, "dram.total_latency", "total memory latency cycles",
                             &total_latency, 0, TRUE, NULL);
    auto& total_burst_st =
            stat_reg_counter(sdb, false, "dram.total_burst", "total memory burst lengths",
                             &total_burst, 0, FALSE, NULL);
    stat_reg_int(sdb, true, "dram.best_latency", "fastest memory latency observed", &best_latency,
                 INT_MAX, FALSE, NULL);
    stat_reg_int(sdb, true, "dram.worst_latency", "worst memory latency observed", &worst_latency,
                 0, FALSE, NULL);
    stat_reg_formula(sdb, true, "dram.average_latency", "average memory latency in cycles",
                     total_latency_st / total_access_st, NULL);
    stat_reg_formula(sdb, true, "dram.average_burst", "average memory burst length",
                     total_burst_st / total_access_st, NULL);
}


#define DRAM_ACCESS_HEADER \
  unsigned int access(const enum cache_command cmd, const md_paddr_t baddr, const int bsize)
#define DRAM_REFRESH_HEADER \
  void refresh(void)
#define DRAM_REG_STATS_HEADER \
  void reg_stats(xiosim::stats::StatsDatabase* sdb)


/* include all of the DRAM definitions */
#include "xiosim/ZCOMPS-dram.list.h"

static std::unique_ptr<dram_t> dram_from_string(const char * const opt_string)
{
  char type[256];

  /* the format string "%[^:]" for scanf reads a string consisting of non-':' characters */
  if(sscanf(opt_string,"%[^:]",type) != 1)
    fatal("malformed dram option string: %s",opt_string);

  /* include the argument parsing code.  DRAM_PARSE_ARGS is defined to
     include only the parsing code and not the other dram code. */
#define DRAM_PARSE_ARGS
#include "xiosim/ZCOMPS-dram.list.h"
#undef DRAM_PARSE_ARGS

  /* UNKNOWN DRAM Type */
  fatal("Unknown dram type (%s)",opt_string);
}

void dram_create(const uncore_knobs_t& knobs)
{
  dram = dram_from_string(knobs.dram_opt_string);
}

