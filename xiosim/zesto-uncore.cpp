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


/* prototype for call to zesto-MC.c */
MC_t * MC_from_string(char * opt_string);

// TODO: Removing static modifier. See zesto-dram for more details.
int fsb_width;    /* in bytes */
bool fsb_DDR;     /* true if DDR */
double fsb_speed; /* in MHz */

double LLC_speed; /* in MHz */
bool fsb_magic; /* ideal FSB flag */
// TODO: Removing static modifier. See zesto-dram for more details.
const char * MC_opt_string = NULL;
const char * LLC_opt_str = "LLC:4096:16:64:16:64:9:L:W:B:8:1:C";
const char * LLC_controller_str = "none";
const char * LLC_MSHR_cmd = "RPWB";
float LLC_magic_hit_rate;

// LLC prefetcher options.
// TODO: Removing static modifier. See zesto-dram for more details.
const char * LLC_PF_opt_str[MAX_PREFETCHERS];
int LLC_num_PF = 0;
int LLC_PFFsize = 8;
int LLC_PFthresh = 2;
int LLC_PFmax = 1;
int LLC_PF_buffer_size = 0;
int LLC_PF_filter_size = 0;
int LLC_PF_filter_reset = 0;
int LLC_WMinterval = 10000;
bool LLC_PF_on_miss = false;
double LLC_low_watermark = 0.1;
double LLC_high_watermark = 0.3;

/* The global pointer to the uncore object */
class uncore_t * uncore = NULL;

/* constructor */
uncore_t::uncore_t(
    const int arg_fsb_width,
    const int arg_fsb_DDR,
    const double arg_fsb_speed,
    const char * MC_opt_string)
: fsb_speed(arg_fsb_speed),
  fsb_DDR(arg_fsb_DDR)
{
  /* temp variables for option-string parsing */
  char name[256];
  int sets, assoc, linesize, latency, banks, bank_width, MSHR_banks, MSHR_entries, MSHR_WB_entries;
  char rp, ap, wp, wc;

  fsb_width = arg_fsb_width;
  fsb_bits = std::log2(fsb_width);
  int llc_ratio = (int)ceil(LLC_speed/fsb_speed);

  fsb = bus_create("FSB", fsb_width, &this->sim_cycle, llc_ratio);
  MC = MC_from_string(MC_opt_string);

  /* Shared LLC */
  if(sscanf(LLC_opt_str,"%[^:]:%d:%d:%d:%d:%d:%d:%c:%c:%c:%d:%d:%d:%c",
      name,&sets,&assoc,&linesize,&banks,&bank_width,&latency,&rp,&ap,&wp, &MSHR_entries, &MSHR_banks, &MSHR_WB_entries, &wc) != 14)
    fatal("invalid LLC options: <name:sets:assoc:linesize:banks:bank-width:latency:repl-policy:alloc-policy:write-policy:num-MSHR:MSHR-banks:WB-buffers:write-combining>\n\t(%s)",LLC_opt_str);

  /* Assume LLC latency is specified in default core clock cycles, since it sounds more natural to users  */
  int latency_scaled = (int) ceil(latency * LLC_speed / knobs.default_cpu_speed);

  LLC = cache_create(NULL,name,CACHE_READWRITE,sets,assoc,linesize,
                     rp,ap,wp,wc,banks,bank_width,latency_scaled,
                     MSHR_entries,MSHR_WB_entries,MSHR_banks,NULL,fsb,
                     LLC_magic_hit_rate);
  if(!LLC_MSHR_cmd || !strcasecmp(LLC_MSHR_cmd,"fcfs"))
    LLC->MSHR_cmd_order = NULL;
  else
  {
    if(strlen(LLC_MSHR_cmd) != 4)
      fatal("-LLC:mshr_cmd must either be \"fcfs\" or contain all four of [RWBP]");
    bool R_seen = false;
    bool W_seen = false;
    bool B_seen = false;
    bool P_seen = false;

    LLC->MSHR_cmd_order = (enum cache_command*)calloc(4,sizeof(enum cache_command));
    if(!LLC->MSHR_cmd_order)
      fatal("failed to calloc MSHR_cmd_order array for LLC");

    for(int c=0;c<4;c++)
    {
      switch(toupper(LLC_MSHR_cmd[c]))
      {
        case 'R': LLC->MSHR_cmd_order[c] = CACHE_READ; R_seen = true; break;
        case 'W': LLC->MSHR_cmd_order[c] = CACHE_WRITE; W_seen = true; break;
        case 'B': LLC->MSHR_cmd_order[c] = CACHE_WRITEBACK; B_seen = true; break;
        case 'P': LLC->MSHR_cmd_order[c] = CACHE_PREFETCH; P_seen = true; break;
        default: fatal("unknown cache operation '%c' for -LLC:mshr_cmd; must be one of [RWBP]");
      }
    }
    if(!R_seen || !W_seen || !B_seen || !P_seen)
      fatal("-LLC:mshr_cmd must contain *each* of [RWBP]");
  }

  LLC->PFF_size = LLC_PFFsize;
  LLC->PFF = (cache_t::PFF_t*) calloc(LLC_PFFsize,sizeof(*LLC->PFF));
  if(!LLC->PFF)
    fatal("failed to calloc %s's prefetch FIFO",LLC->name);
  prefetch_buffer_create(LLC,LLC_PF_buffer_size);
  prefetch_filter_create(LLC,LLC_PF_filter_size,LLC_PF_filter_reset);
  LLC->prefetch_threshold = LLC_PFthresh;
  LLC->prefetch_max = LLC_PFmax;
  LLC->PF_low_watermark = LLC_low_watermark;
  LLC->PF_high_watermark = LLC_high_watermark;
  LLC->PF_sample_interval = LLC_WMinterval;

  LLC->prefetcher = (struct prefetch_t**) calloc(LLC_num_PF?LLC_num_PF:1/* avoid 0-size alloc */,sizeof(*LLC->prefetcher));
  LLC->num_prefetchers = LLC_num_PF;
  if(!LLC->prefetcher)
    fatal("couldn't calloc %s's prefetcher array",LLC->name);
  for(int i=0;i<LLC_num_PF;i++)
    LLC->prefetcher[i] = prefetch_create(LLC_PF_opt_str[i],LLC);
  if(LLC->prefetcher[0] == NULL)
    LLC->num_prefetchers = LLC_num_PF = 0;

  LLC_bus = bus_create("LLC_bus", LLC->linesize*LLC->banks, &this->sim_cycle, 1);
  LLC->controller = controller_create(LLC_controller_str, NULL, LLC);
}

/* destructor */
uncore_t::~uncore_t()
{
  delete(MC);
  MC = NULL;
}

/* register all of the stats */
void uncore_reg_stats(xiosim::stats::StatsDatabase* sdb)
{
  stat_reg_note(sdb,"\n#### LAST-LEVEL CACHE STATS ####");
  stat_reg_counter(sdb, true, "uncore.sim_cycle", "number of uncore cycles simulated", &uncore->sim_cycle, 0, TRUE, NULL);

  LLC_reg_stats(sdb, uncore->LLC);
  bus_reg_stats(sdb, NULL, uncore->LLC_bus);

  stat_reg_note(sdb,"\n#### MAIN MEMORY/DRAM STATS ####");

  dram->reg_stats(sdb);

  bus_reg_stats(sdb, NULL, uncore->fsb);

  uncore->MC->reg_stats(sdb);
}

void uncore_create(void)
{
  uncore = new uncore_t(fsb_width, fsb_DDR, fsb_speed, MC_opt_string);
}
