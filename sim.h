/* sim.h - simulator main line interfaces */

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

#ifndef SIM_H
#define SIM_H

#include <stdio.h>
#include <setjmp.h>
#include <time.h>

#include "options.h"
#include "stats.h"
#include "regs.h"
#include "memory.h"
#include "thread.h"

/* set to non-zero when simulator should dump statistics */
extern int sim_dump_stats;

/* exit when this becomes non-zero */
extern int sim_exit_now;

/* longjmp here when simulation is completed */
extern jmp_buf sim_exit_buf;

/* execution start/end times */
extern int sim_elapsed_time;

/* options database */
extern struct opt_odb_t *sim_odb;

/* stats database */
extern struct stat_sdb_t *sim_sdb;

/* redirected program/simulator output file names */
extern FILE *sim_progfd;

extern struct core_knobs_t knobs;

/*
 * main simulator interfaces, called in the following order
 */

/* register simulator-specific options */
void sim_reg_options(struct opt_odb_t *odb);

/* main() parses options next... */

/* check simulator-specific option values */
void sim_check_options(struct opt_odb_t *odb, int argc, char **argv);

/* register simulator-specific statistics */
void sim_reg_stats(struct thread_t ** cores, struct stat_sdb_t *sdb);

/* initialize the simulator: pre_init gets called before command-line
   argument parsing, post_init gets called after. */
void sim_pre_init(void);
void sim_post_init(void);

/* main() prints the option database values next... */

/* print simulator-specific configuration information */
void sim_aux_config(FILE *stream);

/* start simulation, program loaded, processor precise state initialized */
void sim_main(void);

/* main() prints the stats database values next... */

/* dump simulator-specific auxiliary simulator statistics */
void sim_aux_stats(FILE *stream);

/* un-initialize simulator-specific state */
void sim_uninit(void);

/* print all simulator stats */
void
sim_print_stats(FILE *fd);		/* output stream */

#endif /* SIM_H */
