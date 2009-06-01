/* sim-uop.c - functional uop-level simulator
 * The simulator performs x86-level functional simulation, and then decomposes
 * each x86 macro into a sequence of one or more micro-ops, BUT THE SIMULATOR
 * DOES NOT FUNCTIONALLY EXECUTE EACH UOP.  This is useful for analyzing uop-level
 * dataflow, but does not work for anything that requires uop-level register
 * values (e.g., don't use this to write a sim-cache simulator).
 *
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
 *
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <sys/io.h>

#include "host.h"
#include "misc.h"
#include "thread.h"
#include "loader.h"
#include "syscall.h"
#include "sim.h"

/*******************************************************/

/* simulated processor state */
struct thread_t ** threads = NULL;
int num_threads = 1;

static int max_inst = 0;
static counter_t num_insts = 0;
static counter_t num_uops = 0;

/* register simulator-specific options */
  void
sim_reg_options(struct opt_odb_t *odb)
{
  opt_reg_header(odb, 
      "sim-uop: This simulator executes x86 instructions atomically, but\n"
      "decomposes the insts into uops (micro-ops).\n"
      );

  opt_reg_int(odb, "-max:inst", "uop window size for glob searching",
      &max_inst, /* default */max_inst, /* print */TRUE, /* format */NULL);
}

/* check simulator-specific option values */
  void
sim_check_options(struct opt_odb_t *odb, int argc, char **argv)
{
}

/* register simulator-specific statistics */
  void
sim_reg_stats(struct thread_t ** threads, struct stat_sdb_t *sdb)
{
  stat_reg_counter(sdb, TRUE, "sim_num_insn", "total number of instructions executed", &threads[0]->stat.num_insn, threads[0]->stat.num_insn, NULL);
  stat_reg_int(sdb, TRUE, "sim_elapsed_time", "total simulation time in seconds", &sim_elapsed_time, 0, NULL);
  stat_reg_formula(sdb, TRUE, "sim_inst_rate", "simulation speed (in insts/sec)", "sim_num_insn / sim_elapsed_time", NULL);

  stat_reg_counter(sdb, TRUE, "num_insts", "total number of x86 instructions executed", &num_insts, num_insts, NULL);
  stat_reg_counter(sdb, TRUE, "num_uops", "total number of uops executed", &num_uops, num_uops, NULL);
  stat_reg_formula(sdb, TRUE, "uops_per_inst", "uops per instruction", "num_uops / num_insts", NULL);

  //ld_reg_stats(threads[0],sdb);
  //mem_reg_stats(threads[0]->mem, sdb);
}

/* initialize the simulator */

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

#define DAFLAGS(MSK) (MSK)
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
        ((FAULT) = md_fault_none, MEM_READ_BYTE(thread->mem, /*addr = */(SRC)))
#define READ_WORD(SRC, FAULT)						\
        ((FAULT) = md_fault_none, XMEM_READ_WORD(thread->mem, /*addr = */(SRC)))
#define READ_DWORD(SRC, FAULT)						\
        ((FAULT) = md_fault_none, XMEM_READ_DWORD(thread->mem, /*addr = */(SRC)))
#define READ_QWORD(SRC, FAULT)						\
        ((FAULT) = md_fault_none, XMEM_READ_QWORD(thread->mem, /*addr = */(SRC)))

#define WRITE_BYTE(SRC, DST, FAULT)					\
        ((FAULT) = md_fault_none, MEM_WRITE_BYTE(thread->mem, /*addr = */(DST), (SRC)))
#define WRITE_WORD(SRC, DST, FAULT)					\
        ((FAULT) = md_fault_none, XMEM_WRITE_WORD(thread->mem, /*addr = */(DST), (SRC)))
#define WRITE_DWORD(SRC, DST, FAULT)					\
        ((FAULT) = md_fault_none, XMEM_WRITE_DWORD(thread->mem, /*addr = */(DST), (SRC)))
#define WRITE_QWORD(SRC, DST, FAULT)					\
        ((FAULT) = md_fault_none, XMEM_WRITE_QWORD(thread->mem, /*addr = */(DST), (SRC)))

#else /* !TARGET_X86 */
#error No ISA target defined (only x86 supported)...
#endif /* !TARGET_X86 */

                                                 /* system call handler macro */
#define SYSCALL(INST)	sys_syscall(thread, mem_access, INST, TRUE)

/* Inst/uop execution functions: doing this allows you to actually compile this
   file with optimizations turned on (e.g. gcc -O3), since without it, the
   giant switch was making gcc run out of memory. */
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,I1,I2,I3,OFLAGS,IFLAGS)\
inline void SYMCAT(OP,_IMPL_FUNC)(struct thread_t * thread, struct Mop_t * Mop, struct uop_t * uop, enum md_fault_type * fault, bool * bogus)               \
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


/********************************************************************************************/
/********************************************************************************************/

seq_t global_Mop_seq = 1LL;
seq_t global_uop_seq = 1LL;

inline void uop_init(struct uop_t * uop)
{
  memset(uop,0,sizeof(*uop));
  uop->decode.Mop_seq = (seq_t)-1;
  uop->decode.uop_seq = (seq_t)-1;
}

void
sim_main(void)
{
  struct thread_t * thread = threads[0];
  uop_inst_t flowtab[MD_MAX_FLOWLEN+2];    /* table of uops, +2 for REP control uops */
  bool bogus_v = FALSE;
  bool *bogus = &bogus_v;
  int rep_sequence = 0;
  int i;
  int flow_index = 0;

  fprintf(stderr, "sim: ** starting uop-level functional simulator **\n");

  /* XXX: ignoring endian checking... endian.[ch] needs to be modified to
     handle 64-bit systems */

  while (TRUE)
  {
    struct Mop_t Mop_entry;
    struct Mop_t * Mop = &Mop_entry;
    struct uop_t uop_array[MD_MAX_FLOWLEN+2];
    memset(Mop,0,sizeof(*Mop));
    Mop->uop = uop_array;
    Mop->oracle.seq = global_Mop_seq++;

    /* maintain $r0 semantics */
    thread->regs.regs_R.dw[MD_REG_ZERO] = 0;

    /* load instruction */
    MD_FETCH_INST(Mop->fetch.inst, thread->mem, thread->regs.regs_PC);

    /* decode the instruction */
    MD_SET_OPCODE(Mop->decode.op, Mop->fetch.inst);

    /* convert XCHG X,X to NOPs (XCHG EAX,EAX already identified-as/synonomous-with NOP) */
    if( ((Mop->decode.op == XCHG_RMvRv) || (Mop->decode.op == XCHG_RMbRb)) && (R==RM))
      Mop->decode.op = NOP;
    if(Mop->decode.op == OP_NA)
      Mop->decode.op = NOP;

    Mop->decode.rep_seq = rep_sequence;

    if(Mop->fetch.inst.rep)
      Mop->decode.opflags |= F_COND|F_CTRL;

    Mop->decode.opflags = MD_OP_FLAGS(Mop->decode.op);

    Mop->decode.is_trap = !!(Mop->decode.opflags & F_TRAP);
    Mop->decode.is_ctrl = !!(Mop->decode.opflags & F_CTRL);

    /* set up initial default next PC */
    thread->regs.regs_NPC = thread->regs.regs_PC + MD_INST_SIZE(Mop->fetch.inst);

    if ( REP_FIRST(thread->regs) ) 
    {

      /* execute the instruction */
      switch (Mop->decode.op)
      {
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,I1,I2,I3,OFLAGS,IFLAGS)\
        case OP:							SYMCAT(OP,_IMPL_FUNC)(thread,Mop,NULL,NULL,NULL);						\
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

    /*************************************************************************/
    /* instruction executed; now get the uop flow */

    /* check for insts with repeat count of 0 */
    if(!REP_FIRST(thread->regs))
      Mop->oracle.zero_rep = TRUE;

    /* For the first in a sequence of REPs (including zero REPs), insert
       a microbranch into the flow-table; uop conversion happens later */
    if(Mop->fetch.inst.rep && (rep_sequence == 0))
    {
      Mop->decode.first_rep = TRUE;

      /* inject an initial micro-branch to test if there are any iterations at all */
      if(Mop->fetch.inst.mode & MODE_ADDR32)
        flowtab[0] = (!!(UP) << 30) | (md_uop_opc(REPFIRST_MICROBR_D) << 16);
      else
        flowtab[0] = (!!(UP) << 30) | (md_uop_opc(REPFIRST_MICROBR_W) << 16);

      Mop->decode.flow_length = 1;
    }
    else
      Mop->decode.first_rep = FALSE;

    /* If there are more than zero instances of the instruction, then fill in the uops */
    if(!Mop->oracle.zero_rep || (rep_sequence != 0)) /* a mispredicted zero rep will have rep_sequence != 0 */
    {
      assert(Mop->decode.opflags & F_UCODE); /* all instructions should have a flow mapping (even for 1-uop codes) */

      /* get instruction flow */
      Mop->decode.flow_length += md_get_flow(Mop, flowtab + Mop->decode.first_rep, bogus); /* have to adjust for the micro-branch we may have already injected */

      if(Mop->fetch.inst.rep && (rep_sequence == 0)) /* insert ubranch for 1st iteration of REP */
      {
        struct uop_t * uop = &Mop->uop[0];
        uop_init(uop);
        uop->decode.raw_op = flowtab[0];
        MD_SET_UOPCODE(uop->decode.op,&uop->decode.raw_op);
        uop->decode.opflags = MD_OP_FLAGS(uop->decode.op);
        uop->decode.is_ctrl = TRUE;
        uop->Mop = Mop;
      }

      if((!*bogus) && Mop->decode.flow_length > Mop->decode.first_rep) /* if 1st rep, flow_length already was equal to 1 */
      {
        int imm_uops_left = 0;
        for(i=Mop->decode.first_rep;i<Mop->decode.flow_length;i++)
        {
          struct uop_t * uop = &Mop->uop[i];
          uop_init(uop);
          uop->decode.raw_op = flowtab[i];
          if(!imm_uops_left)
          {
            uop->decode.has_imm = UHASIMM;
            MD_SET_UOPCODE(uop->decode.op,&uop->decode.raw_op);
            uop->decode.opflags = MD_OP_FLAGS(uop->decode.op);
            uop->decode.is_load = !!(uop->decode.opflags & F_LOAD);
            if(Mop->decode.opflags & F_STORE)
            {
              uop->decode.is_sta = !!( (uop->decode.op == STAD) |
                                       (uop->decode.op == STAW) |
                                       (uop->decode.op == STA_BGENW) |
                                       (uop->decode.op == STA_BGENWI) |
                                       (uop->decode.op == STA_BGEND) |
                                       (uop->decode.op == STA_BGENDI)
                                     );
              uop->decode.is_std = !!(uop->decode.opflags & F_STORE);
            }
            uop->decode.is_ctrl = !!(uop->decode.opflags & F_CTRL);
            uop->decode.is_nop = uop->decode.op == NOP;
          }
          else
          {
            imm_uops_left--; /* don't try to decode the immediates! */
            uop->decode.is_imm = TRUE;
          }

          uop->Mop = Mop; /* back-pointer to parent macro-op */
          if(uop->decode.has_imm)
            imm_uops_left = 2;
        }
      }
      else
        fatal("could not locate UCODE flow");

      /* mark repeat iteration for repeating instructions;
         inject corresponding uops (one ECX update, one micro-branch */
      if(Mop->fetch.inst.rep)
      {
        /* ECX/CX update */
        int idx = Mop->decode.flow_length;
        uop_init(&Mop->uop[idx]);
        if(Mop->fetch.inst.mode & MODE_ADDR32)
        {
          flowtab[idx] = ((!!(UP) << 30) | (md_uop_opc(SUBDI) << 16)
                                         | (md_uop_reg(XR_ECX, Mop, bogus) << 12)
                                         | (md_uop_reg(XR_ECX, Mop, bogus) << 8)
                                         | (md_uop_immb(XE_ONE, Mop, bogus)));
          Mop->uop[idx].decode.raw_op = flowtab[idx];
          MD_SET_UOPCODE(Mop->uop[idx].decode.op,&Mop->uop[idx].decode.raw_op);
          Mop->uop[idx].decode.opflags = MD_OP_FLAGS(Mop->uop[idx].decode.op);
          Mop->uop[idx].Mop = Mop;
        }
        else
        {
          flowtab[idx] = ((!!(UP) << 30) | (md_uop_opc(SUBWI) << 16)
                                         | (md_uop_reg(XR_CX, Mop, bogus) << 12)
                                         | (md_uop_reg(XR_CX, Mop, bogus) << 8)
                                         | (md_uop_immb(XE_ONE, Mop, bogus)));
          Mop->uop[idx].decode.raw_op = flowtab[idx];
          MD_SET_UOPCODE(Mop->uop[idx].decode.op,&Mop->uop[idx].decode.raw_op);
          Mop->uop[idx].decode.opflags = MD_OP_FLAGS(Mop->uop[idx].decode.op);
          Mop->uop[idx].Mop = Mop;
        }

        idx++;

        uop_init(&Mop->uop[idx]);

        /* micro-jump to test for end of REP */
        if(Mop->fetch.inst.mode & MODE_ADDR32)
        {
          if(Mop->fetch.inst.rep == REP_REPNZ)
            flowtab[idx] = (!!(UP) << 30) | (md_uop_opc(REPNZ_MICROBR_D) << 16);
          else if(Mop->fetch.inst.rep == REP_REPZ)
            flowtab[idx] = (!!(UP) << 30) | (md_uop_opc(REPZ_MICROBR_D) << 16);
          else if(Mop->fetch.inst.rep == REP_REP)
            flowtab[idx] = (!!(UP) << 30) | (md_uop_opc(REP_MICROBR_D) << 16);
          else
            panic("bogus repeat code");
        }
        else
        {
          if(Mop->fetch.inst.rep == REP_REPNZ)
            flowtab[idx] = (!!(UP) << 30) | (md_uop_opc(REPNZ_MICROBR_W) << 16);
          else if(Mop->fetch.inst.rep == REP_REPZ)
            flowtab[idx] = (!!(UP) << 30) | (md_uop_opc(REPZ_MICROBR_W) << 16);
          else if(Mop->fetch.inst.rep == REP_REP)
            flowtab[idx] = (!!(UP) << 30) | (md_uop_opc(REP_MICROBR_W) << 16);
          else
            panic("bogus repeat code");
        }
        Mop->uop[idx].decode.raw_op = flowtab[idx];
        Mop->uop[idx].decode.is_ctrl = TRUE;
        MD_SET_UOPCODE(Mop->uop[idx].decode.op,&Mop->uop[idx].decode.raw_op);
        Mop->uop[idx].Mop = Mop;

        Mop->decode.flow_length += 2;
      }

      for(i=0;i<Mop->decode.flow_length;i++)
        Mop->uop[i].flow_index = i;
    }
    else /* zero-rep inst */
    {
      struct uop_t * uop = &Mop->uop[0];
      uop_init(uop);
      uop->decode.raw_op = flowtab[0];
      MD_SET_UOPCODE(uop->decode.op,&uop->decode.raw_op);
      uop->decode.opflags = MD_OP_FLAGS(uop->decode.op);
      uop->decode.is_ctrl = TRUE;
      uop->Mop = Mop;
    }

    if(!Mop->fetch.inst.rep || Mop->decode.first_rep)
      Mop->uop[0].decode.BOM = TRUE;

    flow_index = 0;
    while(flow_index < Mop->decode.flow_length)
    {
      /* If we have a microcode op, get the op and inst, this
       * has already been done for non-microcode instructions */
      struct uop_t * uop = &Mop->uop[flow_index];
      uop->decode.FU_class = MD_OP_FUCLASS(uop->decode.op);

      /* get dependency names */
      switch (uop->decode.op)
      {
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,I1,I2,I3,OFLAGS,IFLAGS)\
        case OP: uop->decode.idep_name[0] = I1; uop->decode.odep_name = O1;  \
                 uop->decode.idep_name[1] = I2;  \
                 uop->decode.idep_name[2] = I3;  \
                 uop->decode.iflags = (IFLAGS!=DNA)?IFLAGS:0; \
                 uop->decode.oflags = (OFLAGS!=DNA)?OFLAGS:0; \
        break;
#define DEFUOP(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,I1,I2,I3,OFLAGS,IFLAGS)\
        case OP: uop->decode.idep_name[0] = I1; uop->decode.odep_name = O1;  \
                 uop->decode.idep_name[1] = I2;  \
                 uop->decode.idep_name[2] = I3;  \
                 uop->decode.iflags = (IFLAGS!=DNA)?IFLAGS:0; \
                 uop->decode.oflags = (OFLAGS!=DNA)?OFLAGS:0; \
        break;
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)          \
        case OP:                            \
                              panic("attempted to execute a linking opcode");
#define CONNECT(OP)
#include "machine.def"
        default:
          panic("attempted to execute a bogus opcode");
      }

      uop->decode.Mop_seq = Mop->oracle.seq;
      uop->decode.uop_seq = global_uop_seq++;

      /* check for completed flow at top of loop */
      num_uops++;
      int offset = MD_INC_FLOW;
      Mop->decode.last_uop_index = flow_index;
      flow_index += offset;
    }

    /* if PC==NPC, means we're still REP'ing */
    if(thread->regs.regs_PC == thread->regs.regs_NPC)
    {
      assert(Mop->oracle.spec_mode || Mop->fetch.inst.rep);
      rep_sequence ++;
    }
    else
    {
      Mop->uop[Mop->decode.last_uop_index].decode.EOM = TRUE; /* Mark EOM if appropriate */
      num_insts++;
      rep_sequence = 0;
    }

    /*************************************************************************/

    if(thread->regs.regs_NPC != thread->regs.regs_PC)
    {
      thread->stat.num_insn++; /* only count after all REP's are done; zero REP counts as inst  */
      if(max_inst && (thread->stat.num_insn >= max_inst))
        return;
    }

    /* execute next instruction */
    thread->regs.regs_PC = thread->regs.regs_NPC;
  }
}
