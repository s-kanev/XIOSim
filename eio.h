/* eio.h - external interfaces to external I/O files */

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

#ifndef EIO_H
#define EIO_H

#include <stdio.h>
#include "thread.h"

/* EIO file formats */
#define EIO_X86_FORMAT      2  /* mind you, this is what EIO_ALPHA_FORMAT was also originally defined to */
/* EIO file version */
#define EIO_FILE_VERSION		4

FILE *eio_create(struct thread_t * core, char *fname);

FILE *eio_open(char *fname);

/* returns non-zero if file FNAME has a valid EIO header */
int eio_valid(char *fname);

void eio_close(FILE *fd);

/* check point current architected state to stream FD, returns
   EIO transaction count (an EIO file pointer) */
counter_t
eio_write_chkpt(struct thread_t *core, FILE *fd);			/* stream to write to */

/* read check point of architected state from stream FD, returns
   EIO transaction count (an EIO file pointer) */
counter_t
eio_read_chkpt(struct thread_t *core, FILE *fd);			/* stream to read */

/* syscall proxy handler, with EIO tracing support, architect registers
   and memory are assumed to be precise when this function is called,
   register and memory are updated with the results of the sustem call */
void
eio_write_trace(
    struct thread_t * core,
    FILE *eio_fd,			/* EIO stream file desc */
		mem_access_fn mem_fn,		/* generic memory accessor */
		md_inst_t inst);		/* system call inst */

/* syscall proxy handler from an EIO trace, architect registers
   and memory are assumed to be precise when this function is called,
   register and memory are updated with the results of the sustem call */
#ifdef __cplusplus
extern "C" {
#endif
void
eio_read_trace(
         struct thread_t * core,
         FILE *eio_fd,			/* EIO stream file desc */
	       mem_access_fn mem_fn,		/* generic memory accessor */
	       md_inst_t inst);			/* system call inst */
#ifdef __cplusplus
}
#endif

/* fast forward EIO trace EIO_FD to the transaction just after ICNT */
void eio_fast_forward(struct thread_t * core, FILE *eio_fd, counter_t icnt);

#endif /* EIO_H */
