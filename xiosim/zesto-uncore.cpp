/* zesto-uncore.cpp - Zesto uncore wrapper class
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
#include <limits.h>
#include <ctype.h>

#include "misc.h"
#include "stats.h"
#include "sim.h"

#include "zesto-cache.h"
#include "zesto-prefetch.h"
#include "zesto-uncore.h"
#include "zesto-dram.h"
#include "zesto-MC.h"
#include "zesto-coherence.h"
#include "zesto-noc.h"

/* The uncore class is just a wrapper around the last-level cache,
   front-side-bus and memory controller, plus relevant parameters.
 */

/* The global pointer to the uncore object */
std::unique_ptr<class uncore_t> uncore;

/* constructor */
uncore_t::uncore_t(const uncore_knobs_t& knobs)
    : fsb_speed(knobs.fsb_speed)
    , fsb_DDR(knobs.fsb_DDR) {
    /* temp variables for option-string parsing */
    char name[256];
    int sets, assoc, linesize, latency, banks, bank_width, MSHR_banks, MSHR_entries,
            MSHR_WB_entries;
    char rp, ap, wp, wc;

    fsb_width = knobs.fsb_width;
    fsb_bits = std::log2(knobs.fsb_width);
    int llc_ratio = (int)ceil(knobs.LLC_speed / fsb_speed);

    fsb = bus_create("FSB", fsb_width, &this->sim_cycle, llc_ratio);
    MC = MC_from_string(knobs.MC_opt_string);

    /* Shared LLC */
    if (sscanf(knobs.LLC_opt_str, "%[^:]:%d:%d:%d:%d:%d:%d:%c:%c:%c:%d:%d:%d:%c", name, &sets, &assoc,
               &linesize, &banks, &bank_width, &latency, &rp, &ap, &wp, &MSHR_entries, &MSHR_banks,
               &MSHR_WB_entries, &wc) != 14)
        fatal("invalid LLC options: "
              "<name:sets:assoc:linesize:banks:bank-width:latency:repl-policy:alloc-policy:write-"
              "policy:num-MSHR:MSHR-banks:WB-buffers:write-combining>\n\t(%s)",
              knobs.LLC_opt_str);

    /* Assume LLC latency is specified in default core clock cycles, since it sounds more natural to
     * users  */
    int latency_scaled = (int)ceil(latency * knobs.LLC_speed / core_knobs.default_cpu_speed);

    LLC = cache_create(NULL, name, CACHE_READWRITE, sets, assoc, linesize, rp, ap, wp, wc, banks,
                       bank_width, latency_scaled, MSHR_entries, MSHR_WB_entries, MSHR_banks, NULL,
                       fsb.get(), knobs.LLC_magic_hit_rate, knobs.LLC_MSHR_cmd);

    prefetchers_create(LLC.get(), knobs.LLC_pf);

    LLC_bus = bus_create("LLC_bus", LLC->linesize * LLC->banks, &this->sim_cycle, 1);
    LLC->controller = controller_create(knobs.LLC_controller_str, NULL, LLC.get());
}

/* destructor */
uncore_t::~uncore_t() {}

/* register all of the stats */
void uncore_reg_stats(xiosim::stats::StatsDatabase* sdb) {
    stat_reg_note(sdb, "\n#### LAST-LEVEL CACHE STATS ####");
    stat_reg_counter(sdb, true, "uncore.sim_cycle", "number of uncore cycles simulated",
                     &uncore->sim_cycle, 0, TRUE, NULL);

    LLC_reg_stats(sdb, uncore->LLC.get());
    bus_reg_stats(sdb, NULL, uncore->LLC_bus.get());

    stat_reg_note(sdb, "\n#### MAIN MEMORY/DRAM STATS ####");

    dram->reg_stats(sdb);

    bus_reg_stats(sdb, NULL, uncore->fsb.get());

    uncore->MC->reg_stats(sdb);
}

void uncore_create(const uncore_knobs_t& knobs) { uncore = std::make_unique<uncore_t>(knobs); }
