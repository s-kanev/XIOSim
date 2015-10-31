/* zesto-memdep.cpp - Zesto memory dependence predictor class
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

#include <cmath>

#include "misc.h"
#include "sim.h"
#include "stats.h"
#include "valcheck.h"

#include "zesto-core.h"
#include "zesto-memdep.h"

/*===================================================================*/
/* method implementation for the default memory dependence predictor */
/*===================================================================*/
void memdep_t::init()
{
  bits = 0;
  lookups = 0;
  updates = 0;
  frozen = false;
}

memdep_t::memdep_t(const struct core_t * _core) : core(_core)
{
  init();
  name = strdup("none");
  type = strdup("no memory speculation");
  if(!name || !type)
    fatal("failed to allocate memory for memdep-none (strdup)");
}

memdep_t::~memdep_t()
{
  if(name) free(name);
  if(type) free(type);
  name = NULL; type = NULL;
}

void memdep_t::update(md_addr_t PC)
{
  MEMDEP_STAT(updates++;)
}

void memdep_t::reg_stats(xiosim::stats::StatsDatabase* sdb, struct core_t * core)
{
    using namespace xiosim::stats;
    int coreID = core->id;

    stat_reg_pred_string(sdb, coreID, name, "type", "prediction algorithm of %s", type, NULL);
    auto& bits_st = stat_reg_pred_int(sdb, true, coreID, name, "bits", "total size of %s in bits",
                                      &bits, bits, false, NULL);
    stat_reg_pred_formula(sdb, true, coreID, name, "size", "total size of %s in KB",
                          bits_st / Constant(8192), NULL);
    stat_reg_pred_counter(sdb, true, coreID, name, "lookups",
                          "number of prediction lookups in %s", &lookups, 0, true, NULL);
    stat_reg_pred_counter(sdb, true, coreID, name, "updates",
                          "number of prediction updates in %s", &updates, 0, true, NULL);
}

void memdep_t::freeze_stats(void)
{
  frozen = true;
}

/*======================================================================*/
/* The definitions are all placed in separate files, for organizational
   purposes.                                                            */
/*======================================================================*/

/* COMPONENT BRANCH DIRECTION PREDICTORS
Arguments:
PC              - load's program counter (PC) value
sta_unknown     - are previous sta's unknown?
conflict_exists - oracle answer for whether it's safe for the load to exec
 */
/* Only use this constructor template if you have no arguments */
#define MEMDEP_LOOKUP_HEADER \
  int lookup(const md_addr_t PC, const bool sta_unknown, const bool conflict_exists, const bool partial_match)
#define MEMDEP_UPDATE_HEADER \
  void update(const md_addr_t PC)
#define MEMDEP_REG_STATS_HEADER \
  void reg_stats(xiosim::stats::StatsDatabase* sdb, struct core_t * const core)



#include "xiosim/ZCOMPS-memdep.list.h"


#define MEMDEP_PARSE_ARGS
/*====================================================*/
/* argument parsing                                   */
/*====================================================*/
class memdep_t *
memdep_create(const struct core_t * core, const char * const opt_string)
{
  char name[256];
  char type[256];

  /* the format string "%[^:]" for scanf reads a string consisting of non-':' characters */
  if(sscanf(opt_string,"%[^:]",type) != 1)
    fatal("malformed memdep option string: %s",opt_string);
  /* include the argument parsing code.  MEMDEP_PARSE_ARGS is defined to
     include only the parsing code and not the other branch predictor code. */
#include "xiosim/ZCOMPS-memdep.list.h"


  /* UNKNOWN memdep Type */
  fatal("Unknown memory dependence predictor type (%s)",opt_string);
}

#undef MEMDEP_PARSE_ARGS


