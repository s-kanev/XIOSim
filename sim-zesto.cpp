/* sim-zesto.cpp - x86 execute-at-fetch timing simulator
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
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <sys/io.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "syscall.h"
#include "options.h"
#include "stats.h"
#include "sim.h"
#include "thread.h"

#include "zesto-opts.h"
#include "zesto-core.h"
#include "zesto-oracle.h"
#include "zesto-fetch.h"
#include "zesto-decode.h"
#include "zesto-bpred.h"
#include "zesto-alloc.h"
#include "zesto-exec.h"
#include "zesto-commit.h"
#include "zesto-dram.h"
#include "zesto-uncore.h"
#include "zesto-MC.h"

 
/* architected state */
struct thread_t ** threads = NULL;

/* microarchitecture state */
struct core_t ** cores = NULL;

/* microarchitecture configuration parameters/knobs */
struct core_knobs_t knobs;

/* number of cores */
int num_threads = 1;
int simulated_processes_remaining = 1;

tick_t sim_cycle = 0;

/* initialize simulator data structures - called before any command-line options have been parsed! */
void
sim_pre_init(void)
{
  /* this only sets (malloc) up default values for the knobs */

  memzero(&knobs,sizeof(knobs));

  /* set default parameters */
  knobs.model = "IO-DPM";

  knobs.memory.IL1PF_opt_str[0] = "nextline";
  knobs.memory.IL1_num_PF = 1;

  knobs.fetch.byteQ_size = 4;
  knobs.fetch.byteQ_linesize = 16;
  knobs.fetch.depth = 2;
  knobs.fetch.width = 4;
  knobs.fetch.IQ_size = 8;

  knobs.fetch.bpred_opt_str[0] = "2lev:gshare:1:1024:6:1";
  knobs.fetch.num_bpred_components = 1;

  knobs.decode.depth = 3;
  knobs.decode.width = 4;
  knobs.decode.target_stage = 1;
  knobs.decode.branch_decode_limit = 1;

  knobs.decode.decoders[0] = 4;
  for(int i=1;i<MAX_DECODE_WIDTH;i++)
    knobs.decode.decoders[i] = 1;
  knobs.decode.num_decoder_specs = 4;

  knobs.decode.MS_latency = 0;

  knobs.decode.uopQ_size = 8;

  knobs.alloc.depth = 2;
  knobs.alloc.width = 4;

  knobs.exec.RS_size = 20;
  knobs.exec.LDQ_size = 20;
  knobs.exec.STQ_size = 16;

  knobs.exec.num_exec_ports = 4;
  knobs.exec.payload_depth = 1;
  knobs.exec.fp_penalty = 0;

  knobs.exec.port_binding[FU_IEU].num_FUs = 2;
  knobs.exec.fu_bindings[FU_IEU][0] = 0;
  knobs.exec.fu_bindings[FU_IEU][1] = 1;
  knobs.exec.latency[FU_IEU] = 1;
  knobs.exec.issue_rate[FU_IEU] = 1;

  knobs.exec.port_binding[FU_JEU].num_FUs = 1;
  knobs.exec.fu_bindings[FU_JEU][0] = 0;
  knobs.exec.latency[FU_JEU] = 1;
  knobs.exec.issue_rate[FU_JEU] = 1;

  knobs.exec.port_binding[FU_IMUL].num_FUs = 1;
  knobs.exec.fu_bindings[FU_IMUL][0] = 2;
  knobs.exec.latency[FU_IMUL] = 4;
  knobs.exec.issue_rate[FU_IMUL] = 1;

  knobs.exec.port_binding[FU_SHIFT].num_FUs = 1;
  knobs.exec.fu_bindings[FU_SHIFT][0] = 0;
  knobs.exec.latency[FU_SHIFT] = 1;
  knobs.exec.issue_rate[FU_SHIFT] = 1;

  knobs.exec.port_binding[FU_FADD].num_FUs = 1;
  knobs.exec.fu_bindings[FU_FADD][0] = 0;
  knobs.exec.latency[FU_FADD] = 3;
  knobs.exec.issue_rate[FU_FADD] = 1;

  knobs.exec.port_binding[FU_FMUL].num_FUs = 1;
  knobs.exec.fu_bindings[FU_FMUL][0] = 1;
  knobs.exec.latency[FU_FMUL] = 5;
  knobs.exec.issue_rate[FU_FMUL] = 2;

  knobs.exec.port_binding[FU_FCPLX].num_FUs = 1;
  knobs.exec.fu_bindings[FU_FCPLX][0] = 2;
  knobs.exec.latency[FU_FCPLX] = 58;
  knobs.exec.issue_rate[FU_FCPLX] = 58;

  knobs.exec.port_binding[FU_IDIV].num_FUs = 1;
  knobs.exec.fu_bindings[FU_IDIV][0] = 2;
  knobs.exec.latency[FU_IDIV] = 13;
  knobs.exec.issue_rate[FU_IDIV] = 13;

  knobs.exec.port_binding[FU_FDIV].num_FUs = 1;
  knobs.exec.fu_bindings[FU_FDIV][0] = 2;
  knobs.exec.latency[FU_FDIV] = 32;
  knobs.exec.issue_rate[FU_FDIV] = 24;

  knobs.exec.port_binding[FU_LD].num_FUs = 1;
  knobs.exec.fu_bindings[FU_LD][0] = 1;
  knobs.exec.latency[FU_LD] = 1;
  knobs.exec.issue_rate[FU_LD] = 1;

  knobs.exec.port_binding[FU_STA].num_FUs = 1;
  knobs.exec.fu_bindings[FU_STA][0] = 2;
  knobs.exec.latency[FU_STA] = 1;
  knobs.exec.issue_rate[FU_STA] = 1;

  knobs.exec.port_binding[FU_STD].num_FUs = 1;
  knobs.exec.fu_bindings[FU_STD][0] = 3;
  knobs.exec.latency[FU_STD] = 1;
  knobs.exec.issue_rate[FU_STD] = 1;

  knobs.memory.DL2PF_opt_str[0] = "nextline";
  knobs.memory.DL2_num_PF = 1;
  knobs.memory.DL2_MSHR_cmd = "RPWB";

  knobs.memory.DL1PF_opt_str[0] = "nextline";
  knobs.memory.DL1_num_PF = 1;
  knobs.memory.DL1_MSHR_cmd = "RWBP";

  knobs.commit.ROB_size = 64;
  knobs.commit.width = 4;
}

/* helper signal handler for holding the processor state after a
   seg-fault or deadlock so that a debugger can be attached to check out
   what's wrong. */
void my_SIGSEGV_handler(int signum)
{
#ifdef DEBUG
  fprintf(stderr,"# please attach a debugger or kill the simulator.\n");
  while(true)
    sleep(5);
#else
  fprintf(stderr,"# SIGSEGV or deadlock detected ... killing the simulator\n");
  exit(1);
#endif
}

/* initialize per-thread state, core state, etc. - called AFTER command-line parameters have been parsed */
void
sim_post_init(void)
{
  int i;
  assert(num_threads > 0);

  /* initialize architected state(s) */
  threads = (struct thread_t **)calloc(num_threads,sizeof(*threads));
  if(!threads)
    fatal("failed to calloc threads");
  for(i=0;i<num_threads;i++)
  {
    threads[i] = (struct thread_t *)calloc(1,sizeof(**threads));
    if(!threads[i])
      fatal("failed to calloc threads[%d]",i);

    threads[i]->id = i;
    threads[i]->current_core = i; /* assuming num_threads == num_cores */

    /* allocate and initialize register file */
    regs_init(&threads[i]->regs);

    /* allocate and initialize virtual memory space */
    char buf[128];
    sprintf(buf,"c%d.mem",i);
    threads[i]->mem = mem_create(buf);
    mem_init(threads[i]->mem);
  }

  /* initialize microarchitecture state */
  cores = (struct core_t**) calloc(num_threads,sizeof(*cores));
  if(!cores)
    fatal("failed to calloc cores");
  for(i=0;i<num_threads;i++)
  {
    cores[i] = new core_t(i);
    if(!cores[i])
      fatal("failed to calloc cores[]");

    cores[i]->current_thread = threads[i];
    cores[i]->knobs = &knobs;
  }

  /* install signal handler for debug assistance */
  signal(SIGSEGV,my_SIGSEGV_handler);

  for(i=0;i<num_threads;i++)
  {
    cores[i]->oracle  = new core_oracle_t(cores[i]);
    cores[i]->commit  = commit_create(knobs.model,cores[i]);
    cores[i]->exec  = exec_create(knobs.model,cores[i]);
    cores[i]->alloc  = alloc_create(knobs.model,cores[i]);
    cores[i]->decode  = decode_create(knobs.model,cores[i]);
    cores[i]->fetch  = fetch_create(knobs.model,cores[i]);

    cores[i]->current_thread->active = true;
  }
}

/* load program into simulated state; returns 1 if program is an eio trace */
int
sim_load_prog(
    struct thread_t *thread,
    char *fname,        /* program to load */
    int argc, char **argv,    /* program arguments */
    char **envp)        /* program environment */
{
  /* load program text and data, set up environment, memory, and regs */
  return ld_load_prog(thread, fname, argc, argv, envp);
}

/* print simulator-specific configuration information */
  void
sim_aux_config(FILE *stream)        /* output stream */
{
  /* nothing currently */
}

/* dump simulator-specific auxiliary simulator statistics */
  void
sim_aux_stats(FILE *stream)        /* output stream */
{
  /* nada */
}

/* un-initialize simulator-specific state */
  void
sim_uninit(void)
{
  /* nada */
}



/*
 * The following set of macros are for the fast forwarding
 */

/* next program counter */
#define SET_NPC(EXPR)		(thread->regs.regs_NPC = (EXPR))

/* current program counter */
#define CPC			(thread->regs.regs_PC)

#ifdef TARGET_X86
/* current program counter */
#define CPC			(thread->regs.regs_PC)

/* next program counter */
#define NPC			(thread->regs.regs_NPC)
#define SET_NPC(EXPR)		(thread->regs.regs_NPC = (EXPR))
#define SET_NPC_D(EXPR)         SET_NPC(EXPR)
#define SET_NPC_V(EXPR)							\
  ((Mop->fetch.inst.mode & MODE_OPER32) ? SET_NPC((EXPR)) : SET_NPC((EXPR) & 0xffff))

                                                 /* general purpose registers */
#define _HI(N)			((N) & 0x04)
#define _ID(N)			((N) & 0x03)
#define _ARCH(N)		((N) < MD_REG_TMP0)

                                                 /* segment registers ; UCSD*/
#define SEG_W(N)		(thread->regs.regs_S.w[N])
#define SET_SEG_W(N,EXPR)	(thread->regs.regs_S.w[N] = (EXPR))

#define GPR_B(N)		(_ARCH(N)				\
    ? (_HI(N)				\
      ? thread->regs.regs_R.b[_ID(N)].hi		\
      : thread->regs.regs_R.b[_ID(N)].lo)  	\
    : thread->regs.regs_R.b[N].lo)
#define SET_GPR_B(N,EXPR)	(_ARCH(N)				   \
    ? (_HI(N)				   \
      ? (thread->regs.regs_R.b[_ID(N)].hi = (EXPR))  \
      : (thread->regs.regs_R.b[_ID(N)].lo = (EXPR))) \
    : (thread->regs.regs_R.b[N].lo = (EXPR)))

#define GPR_W(N)		(thread->regs.regs_R.w[N].lo)
#define SET_GPR_W(N,EXPR)	(thread->regs.regs_R.w[N].lo = (EXPR))

#define GPR_D(N)		(thread->regs.regs_R.dw[N])
#define SET_GPR_D(N,EXPR)	(thread->regs.regs_R.dw[N] = (EXPR))

                                                 /* FIXME: move these to the DEF file? */
#define GPR_V(N)		((Mop->fetch.inst.mode & MODE_OPER32)		\
    ? GPR_D(N)				\
    : (dword_t)GPR_W(N))
#define SET_GPR_V(N,EXPR)	((Mop->fetch.inst.mode & MODE_OPER32)		\
    ? SET_GPR_D(N, EXPR)			\
    : SET_GPR_W(N, EXPR))

#define GPR_A(N)		((Mop->fetch.inst.mode & MODE_ADDR32)		\
    ? GPR_D(N)				\
    : (dword_t)GPR_W(N))
#define SET_GPR_A(N,EXPR)	((Mop->fetch.inst.mode & MODE_ADDR32)		\
    ? SET_GPR_D(N, EXPR)			\
    : SET_GPR_W(N, EXPR))

#define GPR_S(N)		((Mop->fetch.inst.mode & MODE_STACK32)		\
    ? GPR_D(N)				\
    : (dword_t)GPR_W(N))
#define SET_GPR_S(N,EXPR)	((Mop->fetch.inst.mode & MODE_STACK32)		\
    ? SET_GPR_D(N, EXPR)			\
    : SET_GPR_W(N, EXPR))

#define GPR(N)                  GPR_D(N)
#define SET_GPR(N,EXPR)         SET_GPR_D(N,EXPR)

#define AFLAGS(MSK)		(thread->regs.regs_C.aflags & (MSK))
#define SET_AFLAGS(EXPR,MSK)	(assert(((EXPR) & ~(MSK)) == 0),	\
    thread->regs.regs_C.aflags =			\
    ((thread->regs.regs_C.aflags & ~(MSK))	\
     | ((EXPR) & (MSK))))

#define FSW(MSK)		(thread->regs.regs_C.fsw & (MSK))
#define SET_FSW(EXPR,MSK)	(assert(((EXPR) & ~(MSK)) == 0),	\
    thread->regs.regs_C.fsw =			\
    ((thread->regs.regs_C.fsw & ~(MSK))		\
     | ((EXPR) & (MSK))))

                                                 // added by cristiano
#define CWD(MSK)                (thread->regs.regs_C.cwd & (MSK))
#define SET_CWD(EXPR,MSK)       (assert(((EXPR) & ~(MSK)) == 0),        \
    thread->regs.regs_C.cwd =                      \
    ((thread->regs.regs_C.cwd & ~(MSK))            \
     | ((EXPR) & (MSK))))

                                                 /* floating point registers, L->word, F->single-prec, D->double-prec */
                                                 /* FIXME: bad overlap, also need to support stack indexing... */
#define _FPARCH(N)		((N) < MD_REG_FTMP0)
#define F2P(N)								\
                                                   (_FPARCH(N)								\
                                                    ? ((FSW_TOP(thread->regs.regs_C.fsw) + (N)) & 0x07)				\
                                                    : (N))
#define FPR(N)			(thread->regs.regs_F.e[F2P(N)])
#define SET_FPR(N,EXPR)		(thread->regs.regs_F.e[F2P(N)] = (EXPR))

#define FPSTACK_POP()							\
                                                   SET_FSW_TOP(thread->regs.regs_C.fsw, (FSW_TOP(thread->regs.regs_C.fsw) + 1) & 0x07)
#define FPSTACK_PUSH()							\
                                                   SET_FSW_TOP(thread->regs.regs_C.fsw, (FSW_TOP(thread->regs.regs_C.fsw) + 7) & 0x07)

#define FPSTACK_OP(OP)							\
{									\
  if ((OP) == fpstk_nop)						\
  /* nada... */;							\
  else if ((OP) == fpstk_pop)						\
  SET_FSW_TOP(thread->regs.regs_C.fsw, (FSW_TOP(thread->regs.regs_C.fsw)+1)&0x07);\
  else if ((OP) == fpstk_poppop)					\
  {									\
    SET_FSW_TOP(thread->regs.regs_C.fsw, (FSW_TOP(thread->regs.regs_C.fsw)+1)&0x07);\
    SET_FSW_TOP(thread->regs.regs_C.fsw, (FSW_TOP(thread->regs.regs_C.fsw)+1)&0x07);\
  }									\
  else if ((OP) == fpstk_push)					\
  SET_FSW_TOP(thread->regs.regs_C.fsw, (FSW_TOP(thread->regs.regs_C.fsw)+7)&0x07);\
  else								\
  panic("bogus FP stack operation");				\
}

#else
#error No ISA target defined (only x86 supported) ...
#endif

                                                 /* precise architected memory state accessor macros */
#ifdef TARGET_X86

#define READ_BYTE(SRC, FAULT)						\
                                                   ((FAULT) = md_fault_none, MEM_READ_BYTE(thread->mem, *addr = (SRC)))
#define READ_WORD(SRC, FAULT)						\
                                                   ((FAULT) = md_fault_none, XMEM_READ_WORD(thread->mem, *addr = (SRC)))
#define READ_DWORD(SRC, FAULT)						\
                                                   ((FAULT) = md_fault_none, XMEM_READ_DWORD(thread->mem, *addr = (SRC)))
#define READ_QWORD(SRC, FAULT)						\
                                                   ((FAULT) = md_fault_none, XMEM_READ_QWORD(thread->mem, *addr = (SRC)))

#define WRITE_BYTE(SRC, DST, FAULT)					\
                                                   ((FAULT) = md_fault_none, MEM_WRITE_BYTE(thread->mem, *addr = (DST), (SRC)))
#define WRITE_WORD(SRC, DST, FAULT)					\
                                                   ((FAULT) = md_fault_none, XMEM_WRITE_WORD(thread->mem, *addr = (DST), (SRC)))
#define WRITE_DWORD(SRC, DST, FAULT)					\
                                                   ((FAULT) = md_fault_none, XMEM_WRITE_DWORD(thread->mem, *addr = (DST), (SRC)))
#define WRITE_QWORD(SRC, DST, FAULT)					\
                                                   ((FAULT) = md_fault_none, XMEM_WRITE_QWORD(thread->mem, *addr = (DST), (SRC)))

#else /* !TARGET_X86 */

#define READ_BYTE(SRC, FAULT)						\
                                                   ((FAULT) = md_fault_none, *addr = (SRC), MEM_READ_BYTE(thread->mem, (SRC)))
#define READ_WORD(SRC, FAULT)						\
                                                   ((FAULT) = md_fault_none, *addr = (SRC), MEM_READ_WORD(thread->mem, (SRC)))
#define READ_DWORD(SRC, FAULT)						\
                                                   ((FAULT) = md_fault_none, *addr = (SRC), MEM_READ_DWORD(thread->mem, (SRC)))
#define READ_QWORD(SRC, FAULT)						\
                                                   ((FAULT) = md_fault_none, *addr = (SRC), MEM_READ_QWORD(thread->mem, (SRC)))

#define WRITE_BYTE(SRC, DST, FAULT)					\
                                                   ((FAULT) = md_fault_none, *addr = (DST), MEM_WRITE_BYTE(thread->mem, (DST), (SRC)))
#define WRITE_WORD(SRC, DST, FAULT)					\
                                                   ((FAULT) = md_fault_none, *addr = (DST), MEM_WRITE_WORD(thread->mem, (DST), (SRC)))
#define WRITE_DWORD(SRC, DST, FAULT)					\
                                                   ((FAULT) = md_fault_none, *addr = (DST), MEM_WRITE_DWORD(thread->mem, (DST), (SRC)))
#define WRITE_QWORD(SRC, DST, FAULT)					\
                                                   ((FAULT) = md_fault_none, *addr = (DST), MEM_WRITE_QWORD(thread->mem, (DST), (SRC)))

#endif /* !TARGET_X86 */

                                                 /* system call handler macro */
#define SYSCALL(INST)							\
                                                    (sys_syscall(thread, mem_access, INST, true))

/* Inst/uop execution functions: doing this allows you to actually compile this
   file with optimizations turned on (e.g. gcc -O3), since without it, the
   giant switch was making gcc run out of memory. */
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,I1,I2,I3,OFLAGS,IFLAGS)\
static inline void SYMCAT(OP,_IMPL_FUNC)(struct thread_t * thread, struct Mop_t * Mop, enum md_fault_type * fault, md_addr_t *addr, bool * bogus)               \
     SYMCAT(OP,_IMPL)
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)
#define CONNECT(OP)
#define DECLARE_FAULT(FAULT)                        \
      { *fault = (FAULT); return; }
#include "machine.def"
#undef DEFINST
#undef DEFUOP
#undef DEFLINK
#undef DECLARE_FAULT

void
sim_fastfwd(struct core_t ** cores, const int insn_count)
{
  md_addr_t addr = 0;
  struct thread_t * thread = NULL;
  enum md_fault_type fault = md_fault_none;
  struct Mop_t Mop_v;
  struct Mop_t * Mop = & Mop_v;
  bool * bogus = NULL;

  memzero(Mop,sizeof(*Mop));
  if(num_threads > 1)
    fprintf(stderr, "### fast forwarding %u instructions per thread, for all %d threads", insn_count,num_threads);
  else
    fprintf(stderr, "### fast forwarding %u instructions", insn_count);
  if(knobs.memory.warm_caches || knobs.fetch.warm_bpred)
  {
    fprintf(stderr,": warming ");
    if(knobs.memory.warm_caches && knobs.fetch.warm_bpred)
      fprintf(stderr,"caches and bpred");
    else if(knobs.memory.warm_caches)
      fprintf(stderr,"caches only");
    else
      fprintf(stderr,"bpred only");
  }
  fprintf(stderr,"\n");

  for(int i=0;i<insn_count;i++)
  {
    for(int t=0;t<num_threads;t++)
    {
      struct core_t * core = cores[t];
      Mop->fetch.bpred_update = core->fetch->bpred->get_state_cache();
      thread = core->current_thread;

exec_rep_again:

      /* maintain $r0 semantics */
      thread->regs.regs_R.dw[MD_REG_ZERO] = 0;

      /* get the next instruction to execute */
      /* read raw bytes from virtual memory */
      MD_FETCH_INST(Mop->fetch.inst, thread->mem, thread->regs.regs_PC);
      /* then decode the instruction */
      MD_SET_OPCODE(Mop->decode.op, Mop->fetch.inst);

      if(Mop->fetch.inst.rep)
        Mop->decode.opflags |= F_COND|F_CTRL;

      Mop->decode.opflags = MD_OP_FLAGS(Mop->decode.op);

      Mop->decode.is_trap = !!(Mop->decode.opflags & F_TRAP);
      Mop->decode.is_ctrl = !!(Mop->decode.opflags & F_CTRL);

      /* set up initial default next PC */
      thread->regs.regs_NPC = thread->regs.regs_PC + MD_INST_SIZE(Mop->fetch.inst);
      Mop->fetch.PC = thread->regs.regs_PC;

      /* set default reference address */
      addr = 0;
      fault = md_fault_none;

      if(REP_FIRST(thread->regs)){

        /* execute the instruction */
        switch (Mop->decode.op)
        {
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,I1,I2,I3,OFLAGS,IFLAGS)\
          case OP:							SYMCAT(OP,_IMPL_FUNC)(thread,Mop,&fault,&addr,bogus);						\
          break;
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)					\
          case OP:							panic("attempted to execute a linking opcode");
#define CONNECT(OP)
#undef DECLARE_FAULT
#define DECLARE_FAULT(FAULT)						\
          { fault = (FAULT); break; }
#include "machine.def"
          default:
            panic("attempted to execute a bogus opcode");
        }

        if (fault != md_fault_none)
          fatal("fault (%d) detected @ 0x%08p", fault, thread->regs.regs_PC);

        /* check for repeating instruction */
        REP_COUNT;
        if(REP_AGAIN(thread->regs)){
          thread->regs.regs_NPC = thread->regs.regs_PC;
        }	    
      }

      if(knobs.memory.warm_caches)
      {
        /* icache */
        md_paddr_t pPC = v2p_translate(thread->id,thread->regs.regs_PC); /* physical addr for PC */
        if(!cache_is_hit(core->memory.IL1,CACHE_READ,pPC,core))
        {
          struct cache_line_t * p = cache_get_evictee(core->memory.IL1,pPC,core);
          p->dirty = p->valid = false;
          cache_insert_block(core->memory.IL1,CACHE_READ,pPC,core);

          if(core->memory.DL2)
          {
            if(!cache_is_hit(core->memory.DL2,CACHE_READ,pPC,core))
            {
              struct cache_line_t * p = cache_get_evictee(core->memory.DL2,pPC,core);
              p->dirty = p->valid = false;
              cache_insert_block(core->memory.DL2,CACHE_READ,pPC,core);

              if(!cache_is_hit(uncore->LLC,CACHE_READ,pPC,core))
              {
                struct cache_line_t * p = cache_get_evictee(uncore->LLC,pPC,core);
                p->dirty = p->valid = false;
                cache_insert_block(uncore->LLC,CACHE_READ,pPC,core);
              }
            }
          }
          else if(!cache_is_hit(uncore->LLC,CACHE_READ,pPC,core))
          {
            struct cache_line_t * p = cache_get_evictee(uncore->LLC,pPC,core);
            p->dirty = p->valid = false;
            cache_insert_block(uncore->LLC,CACHE_READ,pPC,core);
          }
        }
        if(!cache_is_hit(core->memory.ITLB,CACHE_READ,PAGE_TABLE_ADDR(thread->id,thread->regs.regs_PC),core))
        {
          struct cache_line_t * p = cache_get_evictee(core->memory.ITLB,PAGE_TABLE_ADDR(thread->id,thread->regs.regs_PC),core);
          p->dirty = p->valid = false;
          cache_insert_block(core->memory.ITLB,CACHE_READ,PAGE_TABLE_ADDR(thread->id,thread->regs.regs_PC),core);

          if(core->memory.DTLB2 && !cache_is_hit(core->memory.DTLB2,CACHE_READ,PAGE_TABLE_ADDR(thread->id,thread->regs.regs_PC),core))
          {
            struct cache_line_t * p = cache_get_evictee(core->memory.DTLB2,PAGE_TABLE_ADDR(thread->id,thread->regs.regs_PC),core);
            p->dirty = p->valid = false;
            cache_insert_block(core->memory.DTLB2,CACHE_READ,PAGE_TABLE_ADDR(thread->id,thread->regs.regs_PC),core);
          }
        }

        /* dcache */
        if(MD_OP_FLAGS(Mop->decode.op) & F_MEM)
        {
          enum cache_command cmd = CACHE_READ;
          md_paddr_t paddr = v2p_translate(thread->id,addr);
          if(MD_OP_FLAGS(Mop->decode.op) & F_STORE)
            cmd = CACHE_WRITE;
          if(!cache_is_hit(core->memory.DL1,cmd,paddr,core))
          {
            struct cache_line_t * p = cache_get_evictee(core->memory.DL1,paddr,core);
            p->dirty = p->valid = false;
            cache_insert_block(core->memory.DL1,cmd,paddr,core);

            if(core->memory.DL2)
            {
              if(!cache_is_hit(core->memory.DL2,cmd,paddr,core))
              {
                struct cache_line_t * p = cache_get_evictee(core->memory.DL2,paddr,core);
                p->dirty = p->valid = false;
                cache_insert_block(core->memory.DL2,cmd,paddr,core);

                if(!cache_is_hit(uncore->LLC,cmd,paddr,core))
                {
                  struct cache_line_t * p = cache_get_evictee(uncore->LLC,paddr,core);
                  p->dirty = p->valid = false;
                  cache_insert_block(uncore->LLC,cmd,paddr,core);
                }
              }
            }
            else if(!cache_is_hit(uncore->LLC,cmd,paddr,core))
            {
              struct cache_line_t * p = cache_get_evictee(uncore->LLC,paddr,core);
              p->dirty = p->valid = false;
              cache_insert_block(uncore->LLC,cmd,paddr,core);
            }
          }
          if(!cache_is_hit(core->memory.DTLB,CACHE_READ,PAGE_TABLE_ADDR(thread->id,addr),core))
          {
            struct cache_line_t * p = cache_get_evictee(core->memory.DTLB,PAGE_TABLE_ADDR(thread->id,addr),core);
            p->dirty = p->valid = false;
            cache_insert_block(core->memory.DTLB,CACHE_READ,PAGE_TABLE_ADDR(thread->id,addr),core);

            if(core->memory.DTLB2 && !cache_is_hit(core->memory.DTLB2,CACHE_READ,PAGE_TABLE_ADDR(thread->id,addr),core))
            {
              struct cache_line_t * p = cache_get_evictee(core->memory.DTLB2,PAGE_TABLE_ADDR(thread->id,addr),core);
              p->dirty = p->valid = false;
              cache_insert_block(core->memory.DTLB2,CACHE_READ,PAGE_TABLE_ADDR(thread->id,addr),core);
            }
          }
        }
      }

      if(knobs.fetch.warm_bpred && (Mop->decode.is_ctrl || Mop->fetch.inst.rep) && core->fetch->bpred)
      {
        Mop->oracle.NextPC = thread->regs.regs_NPC;
        /* bpred */
        core->fetch->bpred->lookup(Mop->fetch.bpred_update,
                                   Mop->decode.opflags,
                                   Mop->fetch.PC,Mop->fetch.PC+Mop->fetch.inst.len,
                                   Mop->decode.targetPC,
                                   Mop->oracle.NextPC,(Mop->oracle.NextPC != (Mop->fetch.PC+Mop->fetch.inst.len)));

        core->fetch->bpred->recover(Mop->fetch.bpred_update,(Mop->oracle.NextPC != (Mop->fetch.PC + Mop->fetch.inst.len)));

        core->fetch->bpred->update(Mop->fetch.bpred_update,
                                   Mop->decode.opflags,
                                   Mop->fetch.PC,
                                   Mop->fetch.PC + Mop->fetch.inst.len,
                                   Mop->decode.targetPC,
                                   Mop->oracle.NextPC,
                                   (Mop->oracle.NextPC != (Mop->fetch.PC + Mop->fetch.inst.len)));
      }

      /* hit end of inst, count it. (REP counts as single inst.) */
      if(thread->regs.regs_NPC != thread->regs.regs_PC)
        thread->stat.num_insn++;
      else
        goto exec_rep_again; /* complete execution of entire REP instruction before moving on */

      /* go to the next instruction */
      thread->regs.regs_PC = thread->regs.regs_NPC;

      core->fetch->bpred->return_state_cache(Mop->fetch.bpred_update);
    }
  }

  for(int t=0;t<num_threads;t++)
  {
    struct core_t * core = cores[t];
    thread = core->current_thread;

    core->fetch->PC = thread->regs.regs_PC;

    /* maintain $r0 semantics */
    thread->regs.regs_R.dw[MD_REG_ZERO] = 0;

    /* reset stats */
    if(core->memory.IL1) cache_reset_stats(core->memory.IL1);
    if(core->memory.ITLB) cache_reset_stats(core->memory.ITLB);
    if(core->memory.DL1) cache_reset_stats(core->memory.DL1);
    if(core->memory.DL2) cache_reset_stats(core->memory.DL2);
    if(core->memory.DTLB) cache_reset_stats(core->memory.DTLB);
    if(core->memory.DTLB2) cache_reset_stats(core->memory.DTLB2);
    if(core->fetch->bpred) core->fetch->bpred->reset_stats();
    core->stat.eio_commit_insn = insn_count;
  }
  if(uncore->LLC) cache_reset_stats(uncore->LLC);
}

/* If a thread gets really wedged (architecturally incorrect), we can try to recover by blowing away
   all of its state, reloading from its EIO file, and fastfwding back to the point of failure. */
void
emergency_recovery(struct core_t * core)
{
  md_addr_t addr = 0;
  struct thread_t * thread = core->current_thread;
  enum md_fault_type fault = md_fault_none;
  struct Mop_t Mop_v;
  struct Mop_t * Mop = & Mop_v;
  bool * bogus = NULL;
  int insn_count = core->stat.eio_commit_insn;

  if((core->stat.eio_commit_insn == core->last_emergency_recovery_count) && (core->num_emergency_recoveries > 0))
    fatal("After previous attempted recovery, thread %d is still getting wedged... giving up.",core->current_thread->id);

  memset(Mop,0,sizeof(*Mop));
  fprintf(stderr, "### Emergency recovery for thread %d, resetting to inst-count: %lld\n", core->current_thread->id,core->stat.eio_commit_insn);

  /* reset core state */
  core->stat.eio_commit_insn = 0;
  core->oracle->complete_flush();
  core->commit->recover();
  core->exec->recover();
  core->alloc->recover();
  core->decode->recover();
  core->fetch->recover(0);
  wipe_memory(core->current_thread->mem);

  ld_reload_prog(core->current_thread);
  core->fetch->PC = core->current_thread->regs.regs_PC;
  core->fetch->bogus = false;
  core->num_emergency_recoveries++;
  core->last_emergency_recovery_count = core->stat.eio_commit_insn;

  for(int i=0;i<insn_count;i++)
  {
    thread = core->current_thread;

exec_rep_again:

    /* maintain $r0 semantics */
    thread->regs.regs_R.dw[MD_REG_ZERO] = 0;

    /* get the next instruction to execute */
    /* read raw bytes from virtual memory */
    MD_FETCH_INST(Mop->fetch.inst, thread->mem, thread->regs.regs_PC);
    /* then decode the instruction */
    MD_SET_OPCODE(Mop->decode.op, Mop->fetch.inst);

    if(Mop->fetch.inst.rep)
      Mop->decode.opflags |= F_COND|F_CTRL;

    Mop->decode.opflags = MD_OP_FLAGS(Mop->decode.op);

    Mop->decode.is_trap = !!(Mop->decode.opflags & F_TRAP);
    Mop->decode.is_ctrl = !!(Mop->decode.opflags & F_CTRL);

    /* set up initial default next PC */
    thread->regs.regs_NPC = thread->regs.regs_PC + MD_INST_SIZE(Mop->fetch.inst);
    Mop->fetch.PC = thread->regs.regs_PC;

    /* set default reference address */
    addr = 0;
    fault = md_fault_none;

    if(REP_FIRST(thread->regs)){

      /* execute the instruction */
      switch (Mop->decode.op)
      {
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,I1,I2,I3,OFLAGS,IFLAGS)\
        case OP:							SYMCAT(OP,_IMPL_FUNC)(thread,Mop,&fault,&addr,bogus);						\
        break;
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)					\
        case OP:							panic("attempted to execute a linking opcode");
#define CONNECT(OP)
#undef DECLARE_FAULT
#define DECLARE_FAULT(FAULT)						\
        { fault = (FAULT); break; }
#include "machine.def"
        default:
          panic("attempted to execute a bogus opcode");
      }

      if (fault != md_fault_none)
        fatal("fault (%d) detected @ 0x%08p", fault, thread->regs.regs_PC);

      /* check for repeating instruction */
      REP_COUNT;
      if(REP_AGAIN(thread->regs)){
        thread->regs.regs_NPC = thread->regs.regs_PC;
      }	    
    }

    /* hit end of inst, count it. (REP counts as single inst.) */
    if(thread->regs.regs_NPC != thread->regs.regs_PC)
      thread->stat.num_insn++;
    else
      goto exec_rep_again; /* complete execution of entire REP instruction before moving on */

    /* go to the next instruction */
    thread->regs.regs_PC = thread->regs.regs_NPC;
  }

  core->fetch->PC = thread->regs.regs_PC;

  /* maintain $r0 semantics */
  thread->regs.regs_R.dw[MD_REG_ZERO] = 0;

  /* reset stats */
  cache_reset_stats(core->memory.IL1);
  cache_reset_stats(core->memory.ITLB);
  cache_reset_stats(core->memory.DL1);
  cache_reset_stats(uncore->LLC);
  cache_reset_stats(core->memory.DTLB);
  cache_reset_stats(core->memory.DTLB2);
  core->fetch->bpred->reset_stats();
  core->stat.eio_commit_insn = insn_count;

  core->oracle->hosed = false;
}


/* start simulation, program loaded, processor precise state initialized */
void
sim_main(void)
{
  int i;

  for(i=0;i<num_threads;i++)
    cores[i]->fetch->PC = cores[i]->current_thread->regs.regs_PC;

  if(fastfwd)
    sim_fastfwd(cores,fastfwd);

  /* exclude fastforwarding from the simulation wall-clock time */
  sim_start_time = time((time_t *)NULL);

  myfprintf(stderr, "### starting timing simulation \n");

  int start_pos = 0;

  int heartbeat_count = 0;
  while(true)
  {
    sim_cycle++;
    heartbeat_count++;
    for(i=0;i<num_threads;i++)
      if(cores[i]->current_thread->active)
        cores[i]->stat.final_sim_cycle = sim_cycle;

    /*********************************************/
    /* step through pipe stages in reverse order */
    /*********************************************/

    dram->refresh();
    uncore->MC->step();

    step_LLC_PF_controller(uncore);

    for(i=0;i<num_threads;i++)
      step_core_PF_controllers(cores[i]);

    /* all memory processed here */
    for(i=0;i<num_threads;i++)
    {
      /* round-robin on which cache to process first so that one core
         doesn't get continual priority over the others for L2 access */
      cores[mod2m(start_pos+i,num_threads)]->exec->LDST_exec();
    }

    for(i=0;i<num_threads;i++)
      cores[i]->commit->step();

    for(i=0;i<num_threads;i++)
      cores[i]->commit->pre_commit_step();
 
    for(i=0;i<num_threads;i++)
      cores[i]->exec->step();
 
    for(i=0;i<num_threads;i++)
      cores[i]->exec->LDQ_schedule();

    for(i=0;i<num_threads;i++)
      cores[i]->alloc->step();

    for(i=0;i<num_threads;i++)
      cores[i]->decode->step();

    for(i=0;i<num_threads;i++)
    {
      /* round-robin on which cache to process first so that one core
         doesn't get continual priority over the others for L2 access */
      cores[mod2m(start_pos+i,num_threads)]->fetch->step();
    }

    /* this is done last so that prefetch requests have the lowest
       priority when competing for queues, buffers, etc. */
    prefetch_LLC(uncore);

    for(i=0;i<num_threads;i++)
    {
      /* process prefetch requests in reverse order as L1/L2; i.e., whoever
         got the lowest priority for L1/L2 processing gets highest priority
         for prefetch processing */
      prefetch_core_caches(cores[mod2m(start_pos+num_threads-i,num_threads)]);
    }

    /*******************/
    /* occupancy stats */
    /*******************/
    for(i=0;i<num_threads;i++)
    {
      /* this avoids the need to guard each stat update below with "ZESTO_STAT()" */
      if(!cores[i]->current_thread->active)
        continue;

      cores[i]->oracle->update_occupancy();
      cores[i]->fetch->update_occupancy();
      cores[i]->decode->update_occupancy();
      cores[i]->exec->update_occupancy();
      cores[i]->commit->update_occupancy();
    }

    /* check to see if all cores are "ok" */
    for(i=0;i<num_threads;i++)
    {
      if(cores[i]->oracle->hosed)
        emergency_recovery(cores[i]);
    }

    start_pos = modinc(start_pos,num_threads);

    if((heartbeat_frequency > 0) && (heartbeat_count >= heartbeat_frequency))
    {
      fprintf(stderr,"##HEARTBEAT## %lld: {",sim_cycle);
      for(i=0;i<num_threads;i++)
      {
        if(i < (num_threads-1))
          fprintf(stderr,"%lld, ",cores[i]->stat.commit_insn);
        else
          fprintf(stderr,"%lld}\n",cores[i]->stat.commit_insn);
      }
      heartbeat_count = 0;
    }
  }
}

