/* zesto-fetch.cpp - Zesto fetch stage class
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

#include <cstddef>

#include "misc.h"
#include "memory.h"
#include "stats.h"
#include "synchronization.h"
#include "ztrace.h"

#include "zesto-core.h"
#include "zesto-oracle.h"
#include "zesto-fetch.h"
#include "zesto-alloc.h"
#include "zesto-cache.h"
#include "zesto-decode.h"
#include "zesto-bpred.h"
#include "zesto-exec.h"
#include "zesto-commit.h"
#include "zesto-uncore.h"
#include "zesto-coherence.h"


/* default constructor */
core_fetch_t::core_fetch_t(void)
{
}

/* default destructor */
core_fetch_t::~core_fetch_t()
{
}

/* Helper to create all icache and iTLB structures and attach them to core->memory. */
void core_fetch_t::create_caches() {
    struct core_knobs_t* knobs = core->knobs;

    char name[256];
    int sets, assoc, linesize, latency, banks, bank_width, MSHR_entries;
    char rp;

    /* IL1 */
    if (sscanf(knobs->memory.IL1_opt_str, "%[^:]:%d:%d:%d:%d:%d:%d:%c:%d", name, &sets, &assoc,
               &linesize, &banks, &bank_width, &latency, &rp, &MSHR_entries) != 9)
        fatal("invalid IL1 options: "
              "<name:sets:assoc:linesize:banks:bank_width:latency:repl-policy:num-MSHR>\n\t(%s)",
              knobs->memory.IL1_opt_str);

    /* the write-related options don't matter since the IL1 will(should) never see any stores */
    struct cache_t* next_level = (core->memory.DL2) ? core->memory.DL2.get() : uncore->LLC.get();
    struct bus_t* next_bus = (core->memory.DL2) ? core->memory.DL2_bus.get() : uncore->LLC_bus.get();
    core->memory.IL1 = cache_create(core, name, CACHE_READONLY, sets, assoc, linesize, rp, 'w', 't',
                                    'n', banks, bank_width, latency, MSHR_entries, 4, 1, next_level,
                                    next_bus, knobs->memory.IL1_magic_hit_rate,
                                    knobs->memory.IL1_sample_misses, nullptr);

    prefetchers_create(core->memory.IL1.get(), knobs->memory.IL1_pf);

    /* ITLB */
    if (sscanf(knobs->memory.ITLB_opt_str, "%[^:]:%d:%d:%d:%d:%c:%d", name, &sets, &assoc, &banks,
               &latency, &rp, &MSHR_entries) != 7)
        fatal("invalid ITLB options: <name:sets:assoc:banks:latency:repl-policy:num-MSHR>");

    core->memory.ITLB =
            cache_create(core, name, CACHE_READONLY, sets, assoc, 1, rp, 'w', 't', 'n', banks, 1,
                         latency, MSHR_entries, 4, 1, next_level, next_bus, -1.0, false, nullptr);

    core->memory.IL1->controller =
            controller_create(knobs->memory.IL1_controller_opt_str, core, core->memory.IL1.get());
    core->memory.ITLB->controller =
            controller_create(knobs->memory.ITLB_controller_opt_str, core, core->memory.ITLB.get());
}

/* load in all definitions */
#include "xiosim/ZPIPE-fetch.list.h"


std::unique_ptr<class core_fetch_t> fetch_create(const char * fetch_opt_string, struct core_t * core)
{
#define ZESTO_PARSE_ARGS
#include "xiosim/ZPIPE-fetch.list.h"

  fatal("unknown fetch engine type \"%s\"",fetch_opt_string);
#undef ZESTO_PARSE_ARGS
}
