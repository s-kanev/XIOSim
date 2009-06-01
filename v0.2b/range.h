/* range.h - program execution range definitions and interfaces */

/* SimpleScalar(TM) Tool Suite
 * Copyright (C) 1994-2002 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 * All Rights Reserved. 
 * 
 * THIS IS A LEGAL DOCUMENT, BY USING SIMPLESCALAR,
 * YOU ARE AGREEING TO THESE TERMS AND CONDITIONS.
 * 
 * No portion of this work may be used by any commercial entity, or for any
 * commercial purpose, without the prior, written permission of SimpleScalar,
 * LLC (info@simplescalar.com). Nonprofit and noncommercial use is permitted
 * as described below.
 * 
 * 1. SimpleScalar is provided AS IS, with no warranty of any kind, express
 * or implied. The user of the program accepts full responsibility for the
 * application of the program and the use of any results.
 * 
 * 2. Nonprofit and noncommercial use is encouraged. SimpleScalar may be
 * downloaded, compiled, executed, copied, and modified solely for nonprofit,
 * educational, noncommercial research, and noncommercial scholarship
 * purposes provided that this notice in its entirety accompanies all copies.
 * Copies of the modified software can be delivered to persons who use it
 * solely for nonprofit, educational, noncommercial research, and
 * noncommercial scholarship purposes provided that this notice in its
 * entirety accompanies all copies.
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
 * someone who received only the executable form, and is accompanied by a
 * copy of the written offer of source code.
 * 
 * 6. SimpleScalar was developed by Todd M. Austin, Ph.D. The tool suite is
 * currently maintained by SimpleScalar LLC (info@simplescalar.com). US Mail:
 * 2395 Timbercrest Court, Ann Arbor, MI 48105.
 * 
 * Copyright (C) 1994-2002 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 */

#ifndef RANGE_H
#define RANGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include "host.h"
#include "misc.h"
#include "machine.h"

enum range_ptype_t {
  pt_addr = 0,			/* address position */
  pt_inst,			/* instruction count position */
  pt_cycle,			/* cycle count position */
  pt_NUM
};

/*
 * an execution position
 *
 *   by addr:		@<addr>
 *   by inst count:	<icnt>
 *   by cycle count:	#<cycle>
 *
 */
struct range_pos_t {
  enum range_ptype_t ptype;	/* type of position */
  counter_t pos;		/* position */
};

/* an execution range */
struct range_range_t {
  struct range_pos_t start;
  struct range_pos_t end;
};

/* parse execution position *PSTR to *POS */
char *						/* error string, or NULL */
range_parse_pos(
    struct thread_t * core,
    char *pstr,			/* execution position string */
		struct range_pos_t *pos);	/* position return buffer */

/* print execution position *POS */
void
range_print_pos(struct range_pos_t *pos,	/* execution position */
		FILE *stream);			/* output stream */

/* parse execution range *RSTR to *RANGE */
char *						/* error string, or NULL */
range_parse_range(
      struct thread_t * core,
      char *rstr,			/* execution range string */
		  struct range_range_t *range);	/* range return buffer */

/* print execution range *RANGE */
void
range_print_range(struct range_range_t *range,	/* execution range */
		  FILE *stream);		/* output stream */

/* determine if inputs match execution position */
int						/* relation to position */
range_cmp_pos(struct range_pos_t *pos,		/* execution position */
	      counter_t val);			/* position value */

/* determine if inputs are in range */
int						/* relation to range */
range_cmp_range(struct range_range_t *range,	/* execution range */
		counter_t val);			/* position value */


/* determine if inputs are in range, passes all possible info needed */
int						/* relation to range */
range_cmp_range1(struct range_range_t *range,	/* execution range */
		 md_addr_t addr,		/* address value */
		 counter_t icount,		/* instruction count */
		 counter_t cycle);		/* cycle count */

#ifdef __cplusplus
}
#endif

#endif /* RANGE_H */
