/* zesto-prefetch.cpp - Zesto cache prefetcher class
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

#include "sim.h"
#include "stats.h"
#include "valcheck.h"
#include "zesto-opts.h"
#include "zesto-core.h"
#include "zesto-bpred.h"
#include "zesto-cache.h"
#include "zesto-prefetch.h"
#include "zesto-uncore.h"

void prefetch_t::init(void)
{
  cp = NULL;
  type = NULL;
  bits = 0;
  lookups = 0;
}

prefetch_t::prefetch_t(void)
{
  init();
}

prefetch_t::~prefetch_t()
{
  if(type) free(type);
  type = NULL;
}

/* The default prefetcher stats printing function. */
void
prefetch_t::reg_stats(
    struct stat_sdb_t * const sdb,
    const struct core_t * const core)
{
  char buf[256];
  char buf2[256];
  char buf3[256];
  char core_str[256];

  if(core == NULL)
    core_str[0] = 0; /* empty string */
  else
    sprintf(core_str,"c%d.",core->current_thread->id);

  sprintf(buf,"%s%s.%s.bits",core_str,cp->name,type);
  sprintf(buf2,"total size of %s in bits",type);
  stat_reg_int(sdb, true, buf, buf2, &bits, bits, NULL);
  sprintf(buf,"%s%s.%s.size",core_str,cp->name,type);
  sprintf(buf2,"total size of %s in KB",type);
  sprintf(buf3,"%s%s.%s.bits / 8192.0",core_str,cp->name,type);
  stat_reg_formula(sdb, true, buf, buf2, buf3, NULL);
  sprintf(buf,"%s%s.%s.lookups",core_str,cp->name,type);
  sprintf(buf2,"number of prediction lookups in %s",type);
  stat_reg_counter(sdb, true, buf, buf2, &lookups, lookups, NULL);
}

/*========================================================*/
/* The definitions are all placed in separate files, for
  organizational purposes.  */
/*========================================================*/

/*
Arguments:
PC              - load's program counter (PC) value
paddr           - physical address load is reading from
 */
#define PREFETCH_LOOKUP_HEADER \
  md_paddr_t lookup(const md_addr_t PC, const md_paddr_t paddr)
#define PREFETCH_REG_STATS_HEADER \
  void reg_stats(struct stat_sdb_t * const sdb, const struct core_t * const core)



#include "ZCOMPS-prefetch.list"


#define PREFETCH_PARSE_ARGS
/*====================================================*/
/* argument parsing                                   */
/*====================================================*/
  struct prefetch_t *
prefetch_create(const char * const opt_string, struct cache_t * const cp)
{
  char type[256];

  /* the format string "%[^:]" for scanf reads a string consisting of non-':' characters */
  if(sscanf(opt_string,"%[^:]",type) != 1)
    fatal("malformed prefetch option string: %s",opt_string);

  if(!strcasecmp(type,"none"))
    return NULL;
  
  /* include the argument parsing code.  PREFETCH_PARSE_ARGS is defined to
     include only the parsing code and not the other prefetcher code. */
#include "ZCOMPS-prefetch.list"


  /* UNKNOWN prefetch Type */
  fatal("Unknown prefetcher type (%s)",opt_string);
}

#undef PREFETCH_PARSE_ARGS




