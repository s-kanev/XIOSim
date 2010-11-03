/* regs.c - architected registers state routines */
/*
 * SimpleScalar Ô Tool Suite
 * © 1994-2003 Todd M. Austin, Ph.D. and SimpleScalar, LLC
 * All Rights Reserved.
 * 
 * THIS IS A LEGAL DOCUMENT BY DOWNLOADING SIMPLESCALAR, YOU ARE AGREEING TO
 * THESE TERMS AND CONDITIONS.
 * 
 * No portion of this work may be used by any commercial entity, or for any
 * commercial purpose, without the prior, written permission of SimpleScalar,
 * LLC (info@simplescalar.com). Nonprofit and noncommercial use is permitted as
 * described below.
 * 
 * 1. SimpleScalar is provided AS IS, with no warranty of any kind, express or
 * implied. The user of the program accepts full responsibility for the
 * application of the program and the use of any results.
 * 
 * 2. Nonprofit and noncommercial use is encouraged.  SimpleScalar may be
 * downloaded, compiled, executed, copied, and modified solely for nonprofit,
 * educational, noncommercial research, and noncommercial scholarship purposes
 * provided that this notice in its entirety accompanies all copies. Copies of
 * the modified software can be delivered to persons who use it solely for
 * nonprofit, educational, noncommercial research, and noncommercial
 * scholarship purposes provided that this notice in its entirety accompanies
 * all copies.
 * 
 * 3. ALL COMMERCIAL USE, AND ALL USE BY FOR PROFIT ENTITIES, IS EXPRESSLY
 * PROHIBITED WITHOUT A LICENSE FROM SIMPLESCALAR, LLC (info@simplescalar.com).
 * 
 * 4. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 * 
 * 5. Noncommercial and nonprofit users may distribute copies of SimpleScalar
 * in compiled or executable form as set forth in Section 2, provided that
 * either: (A) it is accompanied by the corresponding machine-readable source
 * code, or (B) it is accompanied by a written offer, with no time limit, to
 * give anyone a machine-readable copy of the corresponding source code in
 * return for reimbursement of the cost of distribution. This written offer
 * must permit verbatim duplication by anyone, or (C) it is distributed by
 * someone who received only the executable form, and is accompanied by a copy
 * of the written offer of source code.
 * 
 * 6. SimpleScalar was developed by Todd M. Austin, Ph.D. The tool suite is
 * currently maintained by SimpleScalar LLC (info@simplescalar.com). US Mail:
 * 2395 Timbercrest Court, Ann Arbor, MI 48105.
 * 
 * Copyright © 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "loader.h"
#include "regs.h"

/* create a register file */
struct regs_t *
regs_create(void)
{
  struct regs_t *regs;

  regs = (struct regs_t*) calloc(1, sizeof(struct regs_t));
  if (!regs)
    fatal("out of virtual memory");

  return regs;
}

/* initialize architected register state */
void
regs_init(struct regs_t *regs)		/* register file to initialize */
{
  /* assuming all entries should be zero... */
  /* regs->regs_R[MD_SP_INDEX] and regs->regs_PC initialized by loader... */
  memset(regs, 0, sizeof(*regs));
}

/* Print out the contets of extended-width fp register file */
void
trace_fp_regfile(const md_fpr_t *regs_F, const md_ctrl_t  *regs_C)
{
   int top = FSW_TOP(regs_C->fsw);
   char buff[2*MD_FPR_SIZE+1] = "";
   char tmp[3];

   int j,k;
   unsigned char *curr;

   ZPIN_TRACE("FTOP: %d\n", top)
   for(k=0; k< MD_NUM_ARCH_FREGS; k++)
   {
     curr = (unsigned char*)(&(regs_F->e[k]));
     buff[0] = 0;
     for(j=0; j< MD_FPR_SIZE; j++)
     {
       sprintf(tmp, "%02x", *(curr+(MD_FPR_SIZE-1)-j));
       strncat(buff, tmp, 2);
     }

     ZPIN_TRACE("REG %d: %s \n", k, buff)
     ZPIN_TRACE("%f\n", (double)regs_F->e[k]);
   }
}
