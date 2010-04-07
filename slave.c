/* main.c - main line routines */
/*
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/io.h>

#include "interface.h"
#include "host.h"
#include "misc.h"
#include "machine.h"
#include "endian.h"
#include "version.h"
#include "options.h"
#include "stats.h"
#include "loader.h"
#include "sim.h"

#include "zesto-core.h"
#include "zesto-fetch.h"
#include "zesto-oracle.h"
#include "zesto-decode.h"
#include "zesto-bpred.h"
#include "zesto-alloc.h"
#include "zesto-exec.h"
#include "zesto-commit.h"
#include "zesto-dram.h"
#include "zesto-uncore.h"
#include "zesto-MC.h"


extern void sim_main_slave_pre_pin();
extern void sim_main_slave_post_pin();
extern bool sim_main_slave_fetch_insn();
extern bool sim_main_slave_step();


/* stats signal handler */
extern void signal_sim_stats(int sigtype);


/* exit signal handler */
extern void signal_exit_now(int sigtype);


/* execution start/end times */
extern time_t sim_start_time;
extern time_t sim_end_time;
extern int sim_elapsed_time;

/* byte/word swapping required to execute target executable on this host */
extern int sim_swap_bytes;
extern int sim_swap_words;

/* exit when this becomes non-zero */
extern int sim_exit_now;

/* longjmp here when simulation is completed */
extern jmp_buf sim_exit_buf;

/* set to non-zero when simulator should dump statistics */
extern int sim_dump_stats;

/* options database */
extern struct opt_odb_t *sim_odb;

/* stats database */
extern struct stat_sdb_t *sim_sdb;

/* EIO interfaces */
extern char *sim_eio_fname[MAX_CORES];
extern FILE *sim_eio_fd[MAX_CORES];

/* redirected program/simulator output file names */
extern char *sim_simout;
extern char *sim_progout;
extern FILE *sim_progfd;

/* track first argument orphan, this is the program to execute */
extern int exec_index;

/* dump help information */
extern bool help_me;

/* random number generator seed */
extern int rand_seed;

/* initialize and quit immediately */
extern bool init_quit;

/* simulator scheduling priority */
extern int nice_priority;

/* default simulator scheduling priority */
#define NICE_DEFAULT_VALUE		0

extern int orphan_fn(int i, int argc, char **argv);
extern void banner(FILE *fd, int argc, char **argv);
extern  void usage(FILE *fd, int argc, char **argv);

extern int running;

extern void sim_print_stats(FILE *fd);
extern void exit_now(int exit_code);

unsigned long insn = 0;
unsigned long insn1 = 0;
bool consumed = false;

int
Zesto_SlaveInit(int argc, char **argv)
{
  char *s;
  int i, exit_code;

  /* catch SIGUSR1 and dump intermediate stats */
//SK: Used by PIN
//  signal(SIGUSR1, signal_sim_stats);

  /* catch SIGUSR2 and dump final stats and exit */
//SK: Used by PIN
//  signal(SIGUSR2, signal_exit_now);

  /* register an error handler */
  fatal_hook(sim_print_stats);

  //if(ioperm(0,0xffff,1))
  //if(ioperm(0,0x3ff,1))
    //perror("############ ioperm failed");

  /* set up a non-local exit point */
  if ((exit_code = setjmp(sim_exit_buf)) != 0)
    {
      /* special handling as longjmp cannot pass 0 */
      exit_now(exit_code-1);
    }

  sim_pre_init();

  /* register global options */
  sim_odb = opt_new(orphan_fn);
  opt_reg_flag(sim_odb, "-h", "print help message",
	       &help_me, /* default */FALSE, /* !print */FALSE, NULL);

#ifdef DEBUG
  opt_reg_flag(sim_odb, "-d", "enable debug message",
	       &debugging, /* default */FALSE, /* !print */FALSE, NULL);
#endif /* DEBUG */
  opt_reg_int(sim_odb, "-seed",
	      "random number generator seed (0 for timer seed)",
	      &rand_seed, /* default */1, /* print */TRUE, NULL);
  opt_reg_flag(sim_odb, "-q", "initialize and terminate immediately",
	       &init_quit, /* default */FALSE, /* !print */FALSE, NULL);
  opt_reg_flag(sim_odb, "-ignore_notes", "suppresses printing of notes",
	       &opt_ignore_notes, /* default */FALSE, /* !print */FALSE, NULL);

  /* stdio redirection options */
  opt_reg_string(sim_odb, "-redir:sim",
		 "redirect simulator output to file (non-interactive only)",
		 &sim_simout,
		 /* default */NULL, /* !print */FALSE, NULL);
  opt_reg_string(sim_odb, "-redir:prog",
		 "redirect simulated program output to file",
		 &sim_progout, /* default */NULL, /* !print */FALSE, NULL);

  /* scheduling priority option */
  opt_reg_int(sim_odb, "-nice",
	      "simulator scheduling priority", &nice_priority,
	      /* default */NICE_DEFAULT_VALUE, /* print */TRUE, NULL);

  /* register all simulator-specific options */
  sim_reg_options(sim_odb);

  /* parse simulator options */
  exec_index = -1;
  opt_process_options(sim_odb, argc, argv);

  /* redirect I/O? */
  if (sim_simout != NULL)
    {
      /* send simulator non-interactive output (STDERR) to file SIM_SIMOUT */
      fflush(stderr);
      if (!freopen(sim_simout, "w", stderr))
	fatal("unable to redirect simulator output to file `%s'", sim_simout);
    }

  if (sim_progout != NULL)
    {
      /* redirect simulated program output to file SIM_PROGOUT */
      sim_progfd = fopen(sim_progout, "w");
      if (!sim_progfd)
	fatal("unable to redirect program output to file `%s'", sim_progout);
    }

  /* need at least two argv values to run */
  if (argc < 2)
    {
      banner(stderr, argc, argv);
      usage(stderr, argc, argv);
      exit(1);
    }

  /* opening banner */
  banner(stderr, argc, argv);

  if (help_me)
    {
      /* print help message and exit */
      usage(stderr, argc, argv);
      exit(1);
    }

  /* seed the random number generator */
  if (rand_seed == 0)
    {
      /* seed with the timer value, true random */
      mysrand(time((time_t *)NULL));
    }
  else
    {
      /* seed with default or user-specified random number generator seed */
      mysrand(rand_seed);
    }

  /* exec_index is set in orphan_fn() */
  if (exec_index == -1)
    {
      /* executable was not found */
      fprintf(stderr, "error: no executable specified\n");
      usage(stderr, argc, argv);
      exit(1);
    }
  /* else, exec_index points to simulated program arguments */

  /* check simulator-specific options */
  sim_check_options(sim_odb, argc, argv);

//Irrelevant for slave mode
  /* set simulator scheduling priority */
/*  if (nice(0) < nice_priority)
    {
      if (nice(nice_priority - nice(0)) < 0)
        fatal("could not renice simulator process");
    }*/

  /* initialize the instruction decoder */
  md_init_decoder();

  /* initialize all simulation modules */
  sim_post_init();

  /* register all simulator stats */
  sim_sdb = stat_new();
  sim_reg_stats(threads,sim_sdb);

  /* record start of execution time, used in rate stats */
  sim_start_time = time((time_t *)NULL);

  /* emit the command line for later reuse */
  fprintf(stderr, "sim: command line: ");
  for (i=0; i < argc; i++)
    fprintf(stderr, "%s ", argv[i]);
  fprintf(stderr, "\n");

  /* output simulation conditions */
  s = ctime(&sim_start_time);
  if (s[strlen(s)-1] == '\n')
    s[strlen(s)-1] = '\0';
  fprintf(stderr, "\nsim: simulation started @ %s, options follow:\n", s);
  opt_print_options(sim_odb, stderr, /* short */TRUE, /* notes */TRUE);
  sim_aux_config(stderr);
  fprintf(stderr, "\n");

  /* omit option dump time from rate stats */
  sim_start_time = time((time_t *)NULL);

  if (init_quit)
    exit_now(0);

  running = TRUE;

  /* Run all stages after fetch for first cycle */
  sim_main_slave_pre_pin();

  /* return control to Pin and wait for first instruction */
  return 0;
}

int Zesto_Notify_Mmap(unsigned int addr, unsigned int length)
{
   int i = 0;
   struct mem_t * mem = cores[i]->current_thread->mem;
   assert(num_threads == 1);

   md_addr_t retval = mem_newmap2(mem, ROUND_UP((md_addr_t)addr, MD_PAGE_SIZE), ROUND_UP((md_addr_t)addr, MD_PAGE_SIZE), length, 1);

   myfprintf(stderr, "New memory mapping at addr: %x, length: %u \n",addr, length);
   return (retval == addr);
}

int Zesto_Notify_Munmap(unsigned int addr, unsigned int length)
{
  int i = 0;
  struct mem_t * mem = cores[i]->current_thread->mem;
  assert(num_threads == 1);

  mem_delmap(mem, ROUND_UP((md_addr_t)addr, MD_PAGE_SIZE), length);
  myfprintf(stderr, "Memory un-mapping at addr: %x\n",addr);
  return 1;
}

void Zesto_Destroy()
{
  myfprintf(stderr, "Mops counted in Zesto_Resume: %u\n", insn);
  myfprintf(stderr, "Mops counted in Zesto_Resume loop: %u\n", insn1);

  /* print simulator stats */
  sim_print_stats(stderr);
}


void Zesto_Resume(struct P2Z_HANDSHAKE * handshake)
{
   //TODO: Widen hanshake to include thread id
   assert(num_threads == 1);

   int i = 0;

   regs_t * regs = &cores[i]->current_thread->regs;

   md_addr_t NPC = handshake->brtaken ? handshake->tpc : handshake->npc;  

#ifdef ZESTO_PIN
   myfprintf(stderr, "Getting control from PIN, PC: %x, NPC: %x \n", handshake->pc, NPC);
#endif

   if(insn==0) 
   {  
      cores[i]->current_thread->loader.prog_entry = handshake->pc;

      /* Init stack pointer */
      md_addr_t sp = handshake->ctxt->regs_R.dw[MD_REG_ESP];     
      cores[i]->current_thread->loader.environ_base = sp;

      /* Create local pages for stack 
         XXX: hardcoded 4 pages for now. See how to get stack base + stack endfrom PIN */
      md_addr_t stack_addr = mem_newmap2(cores[i]->current_thread->mem, ROUND_DOWN(sp, MD_PAGE_SIZE), ROUND_DOWN(sp, MD_PAGE_SIZE), 4*MD_PAGE_SIZE, 1);
      myfprintf(stderr, "Stack pointer: %x; \n", sp);


      regs->regs_PC = handshake->pc;
      regs->regs_NPC = handshake->pc;
      cores[i]->fetch->PC = handshake->pc;
   }

   /* Copy architectural state from pim
      XXX: This is arch state BEFORE executed the instruction we're about to simulate*/
 
   regs->regs_R = handshake->ctxt->regs_R;
   regs->regs_F = handshake->ctxt->regs_F;
   regs->regs_C = handshake->ctxt->regs_C;
   regs->regs_S = handshake->ctxt->regs_S;

 
   insn++;
   bool fetch_more = false;
   consumed = false;

//   while(cores[i]->fetch->PC == handshake->pc)
//   while(regs->regs_NPC == handshake->pc)
   while(!consumed)
   {
     fetch_more = sim_main_slave_fetch_insn();

//     regs->regs_NPC = handshake->npc;

     insn1++;

//     if(!cores[i]->oracle->spec_mode )
        /*XXX: here oracle still doesn't know if we're speculating or not. But if we predicted 
        the wrong path, we'd better not return to Pin, because that will mess the state up */
     if(cores[i]->fetch->PC != NPC
           && cores[i]->fetch->PC != handshake->pc) //Not trapped
     {
       do
       {
         while(fetch_more && cores[i]->fetch->PC != NPC)
           fetch_more = sim_main_slave_fetch_insn();
        
         sim_main_slave_post_pin();

         /* Next cycle */ 
         sim_main_slave_pre_pin();

       }while(cores[i]->fetch->PC != NPC || cores[i]->oracle->spec_mode);
//       }while(cores[i]->oracle->spec_mode);

      /* After we recover from a speculation, we still need to execute the instruction Pin called us about */
//      consumed = false;
       regs->regs_NPC = NPC;
       return; // ? if we cam utilize a new PC
     }
     else
     /* non-speculative */
     {
       /* Pass control back to Pin to get a new PC on the same cycle*/
       if(fetch_more)// && (cores[i]->fetch->PC == handshake->npc))
       {
         regs->regs_NPC = NPC; 
         return;
       }

//       if(cores[i]->fetch->trapped)
//       {
//         regs->regs_NPC = NPC;
//         return;
//       }
      
       sim_main_slave_post_pin();

       /* This is already next cycle, up to fetch */
       sim_main_slave_pre_pin();
     }
   }
}


