/* sim-cache.c - simple in-order/functional cache simulator
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
#include <sys/io.h>

#include "host.h"
#include "misc.h"
#include "thread.h"
#include "loader.h"
#include "syscall.h"
#include "sim.h"

#include "zesto-core.h"
#include "zesto-cache.h"
#include "zesto-uncore.h"

tick_t sim_cycle = 0;
struct uncore_t * uncore = NULL; // for linking purposes
struct core_t * core = NULL;

/*******************************************************/

/* simulated processor state */
struct thread_t ** threads = NULL;
int num_threads = 1;

long long fastfwd = 0;
long long max_insts = 0;

char * il1_opt_str = NULL;
char * dl1_opt_str = NULL;
char * LLC_opt_str = NULL;

cache_t * IL1 = NULL;
cache_t * DL1 = NULL;
cache_t * LLC = NULL;

/* register simulator-specific options */
  void
sim_reg_options(struct opt_odb_t *odb)
{
  core = new core_t(0);
  core->current_thread = threads[0];

  opt_reg_header(odb, 
      "sim-cache: This simulator implements a simple in-order function simulator\n"
      "for quickly modeling cache behaviors.  All instructions are looked up in\n"
      "the IL1 cache, and all loads/stores in the DL1.  Misses in either get\n"
      "forwarded to the last-level cache (LCC, A.K.A. the L2).  Some stats are\n"
      "not properly printed out because they require timing information (cycle-\n"
      "accurate simulation).\n"
      );
  opt_reg_long_long(odb, "-fastfwd", "number of inst's to skip before timing simulation",
      &fastfwd, /* default */0, /* print */true, /* format */NULL);
  opt_reg_long_long(odb, "-max:inst", "maximum number of inst's to execute",
      &max_insts, /* default */0,
      /* print */true, /* format */NULL);

  opt_reg_string(odb, "-il1","IL1 config string",
      &il1_opt_str, /*default*/ "IL1:64:8:64:4:16:3:L:8", /*print*/true,/*format*/NULL);
  opt_reg_string(odb, "-dl1","DL1 config string",
      &dl1_opt_str, /*default*/ "DL1:64:8:64:8:16:2:L:W:T:8:8:C", /*print*/true,/*format*/NULL);
  opt_reg_string(odb, "-LLC","LLC config string",
      &LLC_opt_str, /*default*/ "LLC:4096:16:64:16:64:9:L:W:B:8:1:8:C", /*print*/true,/*format*/NULL);
}

/* check simulator-specific option values */
  void
sim_check_options(struct opt_odb_t *odb, int argc, char **argv)
{
  char name[256];
  int sets, assoc, linesize, latency, banks, bank_width;
  char rp, ap, wp;


  /* IL1 */
  if(sscanf(il1_opt_str,"%[^:]:%d:%d:%d:%d:%d:%d:%c",
      name,&sets,&assoc,&linesize,&banks,&bank_width,&latency, &rp) != 8)
    fatal("invalid IL1 options: <name:sets:assoc:linesize:banks:bank_width:latency:repl-policy>\n\t(%s)",il1_opt_str);

  /* the write-related options don't matter since the IL1 will(should) never see any stores */
  IL1 = cache_create(NULL,name,CACHE_READONLY,sets,assoc,linesize,rp,'w','t','n',banks,bank_width,latency,4,4,1,NULL,NULL);


  /* DL1 */
  if(sscanf(dl1_opt_str,"%[^:]:%d:%d:%d:%d:%d:%d:%c:%c:%c",
      name,&sets,&assoc,&linesize,&banks,&bank_width,&latency,&rp,&ap,&wp) != 10)
    fatal("invalid DL1 options: <name:sets:assoc:linesize:banks:bank-width:latency:repl-policy:alloc-policy:write-policy>\n\t(%s)",dl1_opt_str);

  DL1 = cache_create(NULL,name,CACHE_READWRITE,sets,assoc,linesize,
                                   rp,ap,wp,'c',banks,bank_width,latency,
                                   4,4,1,NULL,NULL);

  /* LLC */
  if(sscanf(LLC_opt_str,"%[^:]:%d:%d:%d:%d:%d:%d:%c:%c:%c",
      name,&sets,&assoc,&linesize,&banks,&bank_width,&latency,&rp,&ap,&wp) != 10)
    fatal("invalid LLC options: <name:sets:assoc:linesize:banks:bank-width:latency:repl-policy:alloc-policy:write-policy>\n\t(%s)",LLC_opt_str);

  LLC = cache_create(NULL,name,CACHE_READWRITE,sets,assoc,linesize,
                     rp,ap,wp,'c',banks,bank_width,latency,
                     4,4,1,NULL,NULL);
}

/* register simulator-specific statistics */
  void
sim_reg_stats(struct thread_t ** threads, struct stat_sdb_t *sdb)
{
  stat_reg_counter(sdb, TRUE, "sim_num_insn",
      "total number of instructions executed",
      &threads[0]->stat.num_insn, threads[0]->stat.num_insn, NULL);
  stat_reg_int(sdb, TRUE, "sim_elapsed_time",
      "total simulation time in seconds",
      &sim_elapsed_time, 0, NULL);

  stat_reg_formula(sdb, FALSE, "c0.commit_insn",
      "number of committed insn",
      "sim_num_insn", NULL);
  stat_reg_formula(sdb, TRUE, "sim_inst_rate",
      "simulation speed (in insts/sec)",
      "sim_num_insn / sim_elapsed_time", NULL);
  cache_reg_stats(sdb,core,IL1);
  cache_reg_stats(sdb,core,DL1);
  LLC_reg_stats(sdb,LLC);
  //ld_reg_stats(threads[0],sdb);
  mem_reg_stats(threads[0]->mem, sdb);
}

/* called before command-line parameter parsing */
void
sim_pre_init(void)
{
  threads = (struct thread_t**) calloc(1,sizeof(*threads));
  if(!threads)
    fatal("failed to calloc threads");
  threads[0] = (struct thread_t*) calloc(1,sizeof(**threads));
  if(!threads[0])
    fatal("failed to calloc threads");

  /* allocate and initialize register file */
  regs_init(&threads[0]->regs);

  /* allocate and initialize memory space */
  threads[0]->mem = mem_create("c0.mem");
  mem_init(threads[0]->mem);
}

/* called after command-line parameter parsing */
void
sim_post_init(void)
{
  /* nada */
}

/* load program into simulated state; returns 1 if program is an eio trace */
int
sim_load_prog(
    struct thread_t * thread,
    char *fname,		/* program to load */
    int argc, char **argv,	/* program arguments */
    char **envp)		/* program environment */
{
  /* load program text and data, set up environment, memory, and regs */
  return ld_load_prog(thread, fname, argc, argv, envp);
}

/* print simulator-specific configuration information */
  void
sim_aux_config(FILE *stream)
{
  /* nada */
}

/* dump simulator-specific auxiliary simulator statistics */
  void
sim_aux_stats(FILE *stream)
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
 * configure the execution engine
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

                                                 // direct references, no top of the stack indirection used
                                                 //#define FPR_NS(N)			(thread->regs.regs_F.e[(N)])
                                                 //#define SET_FPR_NS(N,EXPR)		(thread->regs.regs_F.e[(N)] = (EXPR))

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
#error No ISA target defined (only x86 supported)...
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
#error No ISA target defined (only x86 supported)...
#endif /* !TARGET_X86 */

                                                 /* system call handler macro */
#define SYSCALL(INST)	sys_syscall(thread, mem_access, INST, TRUE)

/* Inst/uop execution functions: doing this allows you to actually compile this
   file with optimizations turned on (e.g. gcc -O3), since without it, the
   giant switch was making gcc run out of memory. */
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,I1,I2,I3,OFLAGS,IFLAGS)\
inline void SYMCAT(OP,_IMPL_FUNC)(struct thread_t * thread, struct Mop_t * Mop, struct uop_t * uop, enum md_fault_type * fault, md_addr_t *addr, bool * bogus)               \
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

                                                 /* start simulation, program loaded, processor precise state initialized */
  void
sim_main(void)
{
  struct thread_t * thread = threads[0];
  struct Mop_t Mop_v;
  struct Mop_t * Mop = &Mop_v;
  enum md_fault_type fault = md_fault_none;
  md_addr_t addr = 0;
  bool * bogus = NULL;

  /* TODO: add fastfwd and maxinst */

  fprintf(stderr, "sim: ** starting *fast* functional simulation **\n");

  /* must have natural byte/word ordering */

  int state;
  long long insn_count;

  if(fastfwd)
  {
    state = 0;
    insn_count = fastfwd;
  }
  else
  {
    state = 1;
    if(max_insts)
      insn_count = max_insts;
    else
      insn_count = TICK_T_MAX;
  }

  while(1)
  {
    for(long long i=0;i<insn_count;i++)
    {
      /* maintain $r0 semantics */
      thread->regs.regs_R.dw[MD_REG_ZERO] = 0;

      /* load instruction */
      MD_FETCH_INST(Mop->fetch.inst, thread->mem, thread->regs.regs_PC);

      /* decode the instruction */
      MD_SET_OPCODE(Mop->decode.op, Mop->fetch.inst);

      /* set up initial default next PC */
      thread->regs.regs_NPC = thread->regs.regs_PC + MD_INST_SIZE(Mop->fetch.inst);

      /* set default reference address */
      addr = 0;
      fault = md_fault_none;

      if ( REP_FIRST(thread->regs) ) 
      {

        /* execute the instruction */
        switch (Mop->decode.op)
        {
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,I1,I2,I3,OFLAGS,IFLAGS)\
          case OP:							SYMCAT(OP,_IMPL_FUNC)(thread,Mop,NULL,&fault,&addr,bogus);						\
          break;
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)					\
          case OP:							panic("attempted to execute a linking opcode");
#define CONNECT(OP)
#define DECLARE_FAULT(FAULT)						\
          { /* uncaught... */break; }
#include "machine.def"
          default:
            panic("attempted to execute a bogus opcode");
        }

        /* check for repeating instruction */
        REP_COUNT;
        if(REP_AGAIN(thread->regs)){
          thread->regs.regs_NPC = thread->regs.regs_PC;
        }
      }

      /* icache */
      md_paddr_t pPC = v2p_translate(thread->id,thread->regs.regs_PC); /* physical addr for PC */
      IL1->stat.load_lookups++;
      if(!cache_is_hit(IL1,CACHE_READ,pPC,core))
      {
        IL1->stat.load_misses++;
        struct cache_line_t * p = cache_get_evictee(IL1,pPC,core);
        p->dirty = p->valid = FALSE;
        cache_insert_block(IL1,CACHE_READ,pPC,core);

        LLC->stat.core_lookups[0]++;
        LLC->stat.load_lookups++;
        if(!cache_is_hit(LLC,CACHE_READ,pPC,core))
        {
          LLC->stat.core_misses[0]++;
          LLC->stat.load_misses++;
          struct cache_line_t * p = cache_get_evictee(LLC,pPC,core);
          p->dirty = p->valid = FALSE;
          cache_insert_block(LLC,CACHE_READ,pPC,core);
        }
      }

      /* dcache */
      if(MD_OP_FLAGS(Mop->decode.op) & F_MEM)
      {
        enum cache_command cmd = CACHE_READ;
        md_paddr_t paddr = v2p_translate(thread->id,addr);
        if(MD_OP_FLAGS(Mop->decode.op) & F_STORE)
        {
          DL1->stat.store_lookups++;
          cmd = CACHE_WRITE;
        }
        else
          DL1->stat.load_lookups++;

        if(!cache_is_hit(DL1,cmd,paddr,core))
        {
          if(cmd == CACHE_READ)
          {
            DL1->stat.load_misses++;
            LLC->stat.load_lookups++;
          }
          else
          {
            DL1->stat.store_misses++;
            LLC->stat.store_lookups++;
          }
          struct cache_line_t * p = cache_get_evictee(DL1,paddr,core);
          p->dirty = p->valid = FALSE;
          cache_insert_block(DL1,cmd,paddr,core);

          LLC->stat.core_lookups[0]++;

          if(!cache_is_hit(LLC,cmd,paddr,core))
          {
            struct cache_line_t * p = cache_get_evictee(LLC,paddr,core);
            p->dirty = p->valid = FALSE;
            cache_insert_block(LLC,cmd,paddr,core);

            LLC->stat.core_misses[0]++;
            if(cmd == CACHE_READ)
              LLC->stat.load_misses++;
            else
              LLC->stat.store_misses++;
          }
        }
      }

      if(thread->regs.regs_NPC != thread->regs.regs_PC)
        thread->stat.num_insn++; /* only count after all REP's are done; zero REP counts as inst  */

      /* execute next instruction */
      thread->regs.regs_PC = thread->regs.regs_NPC;
    }
    if(state==0) /* just finish fastfwd'ing */
    {
      state = 1;
      if(max_insts)
        insn_count = max_insts;
      else
        insn_count = TICK_T_MAX;
      cache_reset_stats(IL1);
      cache_reset_stats(DL1);
      cache_reset_stats(LLC);
    }
    else
      break; /* done */
  }
}
