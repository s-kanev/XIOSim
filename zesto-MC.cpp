/* zesto-MC.cpp - Zesto memory controller class
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

#include <limits.h>
#include "thread.h"
#include "stats.h"
#include "options.h"
#include "zesto-opts.h"
#include "zesto-cache.h"
#include "zesto-uncore.h"
#include "zesto-MC.h"
#include "zesto-dram.h"



/* register default memory-controller stats */
void MC_t::reg_stats(struct stat_sdb_t * sdb)
{
  stat_reg_counter(sdb, true, "MC.total_accesses", "total accesses to memory controller",
      &total_accesses, /* initial value */0, /* format */NULL);
  stat_reg_counter(sdb, false, "MC.total_service_cycles", "cumulative request service cycles",
      &total_service_cycles, /* initial value */0, /* format */NULL);
  stat_reg_formula(sdb, true, "MC.service_average","average cycles per MC request",
      "MC.total_service_cycles / MC.total_accesses",/* format */NULL);
}

void MC_t::init(void)
{
  total_accesses = 0;
  total_dram_cycles = 0;
  total_service_cycles = 0;
}

void MC_t::reset_stats(void)
{
  total_accesses = 0;
  total_dram_cycles = 0;
  total_service_cycles = 0;
}

#define MC_ENQUEUABLE_HEADER \
  bool enqueuable(void)
#define MC_ENQUEUE_HEADER \
  void enqueue(struct cache_t * const prev_cp,  \
               const enum cache_command cmd,  \
               const md_paddr_t addr,  \
               const int linesize,  \
               const seq_t action_id,  \
               const int MSHR_bank,  \
               const int MSHR_index,  \
               void * const op,  \
               void (*const cb)(void *),  \
               seq_t (*const get_action_id)(void *) )
#define MC_STEP_HEADER \
  void step(void)
#define MC_REG_STATS_HEADER \
  void reg_stats(struct stat_sdb_t * const sdb)
#define MC_RESET_STATS_HEADER \
  void reset_stats(void)
#define MC_PRINT_HEADER \
  void print(FILE * const fp)

#include "ZCOMPS-MC.list"

MC_t * MC_from_string(char * const opt_string)
{
  char type[256];

  /* the format string "%[^:]" for scanf reads a string consisting of non-':' characters */
  if(sscanf(opt_string,"%[^:]",type) != 1)
    fatal("malformed dram option string: %s",opt_string);

  /* include the argument parsing code.  MC_PARSE_ARGS is defined to
     include only the parsing code and not the other dram code. */
#define MC_PARSE_ARGS
#include "ZCOMPS-MC.list"
#undef MC_PARSE_ARGS

  /* UNKNOWN Memory-Controller Type */
  fatal("Unknown memory-controller type (%s)",opt_string);
}
