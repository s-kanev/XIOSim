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

extern tick_t sim_cycle;

bool consumed = false;
bool first_insn = true;
long long fetches_since_feeder = 0;

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

void Zesto_SetBOS(unsigned int stack_base)
{
   assert(num_threads == 1);
   cores[0]->current_thread->loader.stack_base = (md_addr_t)stack_base;
   myfprintf(stderr, "Stack base: %x; \n", cores[0]->current_thread->loader.stack_base);

}

int Zesto_Notify_Mmap(unsigned int addr, unsigned int length, bool mod_brk)
{
   int i = 0;
   struct core_t * core = cores[i];
   struct mem_t * mem = cores[i]->current_thread->mem;
   assert(num_threads == 1);

   md_addr_t page_addr = ROUND_DOWN((md_addr_t)addr, MD_PAGE_SIZE);
   unsigned int page_length = ROUND_UP(length, MD_PAGE_SIZE);

   md_addr_t retval = mem_newmap2(mem, page_addr, page_addr, page_length, 1);

//   myfprintf(stderr, "New memory mapping at addr: %x, length: %x ,endaddr: %x \n",addr, length, addr+length);
   ZPIN_TRACE("New memory mapping at addr: %x, length: %x ,endaddr: %x \n",addr, length, addr+length)

   bool success = (retval == addr);
   zesto_assert(success, 0);

   if(mod_brk && page_addr > cores[i]->current_thread->loader.brk_point)
     cores[i]->current_thread->loader.brk_point = page_addr + page_length;

   return success;
}

int Zesto_Notify_Munmap(unsigned int addr, unsigned int length, bool mod_brk)
{
  int i = 0;
  struct mem_t * mem = cores[i]->current_thread->mem;
  assert(num_threads == 1);

  mem_delmap(mem, ROUND_UP((md_addr_t)addr, MD_PAGE_SIZE), length);

//  myfprintf(stderr, "Memory un-mapping at addr: %x, len: %x\n",addr, length);
  ZPIN_TRACE("Memory un-mapping at addr: %x, len: %x\n",addr, length)

  return 1;
}

void Zesto_UpdateBrk(unsigned int brk_end, bool do_mmap)
{
  int i = 0;
  struct core_t * core = cores[i];

  assert(num_threads == 1);
  zesto_assert(num_threads == 1, (void)0);

  zesto_assert(brk_end != 0, (void)0);

  if(do_mmap)
  {
    unsigned int old_brk_end = cores[0]->current_thread->loader.brk_point;

    if(brk_end > old_brk_end)
      Zesto_Notify_Mmap(ROUND_UP(old_brk_end, MD_PAGE_SIZE), 
                        ROUND_UP(brk_end - old_brk_end, MD_PAGE_SIZE), false);
    else if(brk_end < old_brk_end)
      Zesto_Notify_Munmap(ROUND_UP(brk_end, MD_PAGE_SIZE),
                          ROUND_UP(old_brk_end - brk_end, MD_PAGE_SIZE), false);
  }

  core->current_thread->loader.brk_point = brk_end;
}

void Zesto_Destroy()
{
  /* print simulator stats */
  sim_print_stats(stderr);
}


void Zesto_Resume(struct P2Z_HANDSHAKE * handshake, bool start_slice, bool end_slice)
{
   //TODO: Widen hanshake to include thread id
   assert(num_threads == 1);

   int i = 0;
   struct core_t * core = cores[i];

   thread_t * thread = cores[i]->current_thread;
   regs_t * regs = &thread->regs;

   md_addr_t NPC = handshake->brtaken ? handshake->tpc : handshake->npc;  

   zesto_assert(cores[i]->oracle->num_Mops_nuked == 0, (void)0);
   zesto_assert(!cores[i]->oracle->spec_mode, (void)0);
   zesto_assert(thread->rep_sequence == 0, (void)0);

   cores[i]->fetch->feeder_NPC = NPC;
   cores[i]->fetch->feeder_PC = handshake->pc;

   if(first_insn) 
   {  
      thread->loader.prog_entry = handshake->pc;

      first_insn= false;
   }

   if(start_slice)
   {
      zesto_assert(thread->loader.stack_base, (void)0);

      /* Init stack pointer */
      md_addr_t sp = handshake->ctxt->regs_R.dw[MD_REG_ESP]; 
      thread->loader.stack_size = thread->loader.stack_base-sp;
      thread->loader.stack_min = (md_addr_t)sp;

      /* Create local pages for stack */ 
      md_addr_t page_start = ROUND_DOWN(sp, MD_PAGE_SIZE);
      md_addr_t page_end = ROUND_UP(thread->loader.stack_base, MD_PAGE_SIZE);

      md_addr_t stack_addr = mem_newmap2(thread->mem, page_start, page_start, page_end-page_start, 1);
      myfprintf(stderr, "Stack pointer: %x; \n", sp);
      zesto_assert(stack_addr == ROUND_DOWN(thread->loader.stack_min, MD_PAGE_SIZE), (void)0);


      regs->regs_PC = handshake->pc;
      regs->regs_NPC = handshake->pc;
      cores[i]->fetch->PC = handshake->pc;
   }

   if(end_slice)
   {
      Zesto_Drain();
      return;
   }

   ZPIN_TRACE("PIN -> PC: %x, NPC: %x \n", handshake->pc, NPC)
   fetches_since_feeder = 0;

   /* Copy architectural state from pin
      XXX: This is arch state BEFORE executed the instruction we're about to simulate*/

   regs->regs_R = handshake->ctxt->regs_R;
   regs->regs_C = handshake->ctxt->regs_C;
   regs->regs_S = handshake->ctxt->regs_S;

   /* Copy only valid FP registers (PIN uses invalid ones and they may differ) */
   int j;
   for(j=0; j< MD_NUM_ARCH_FREGS; j++)
     if(FPR_VALID(handshake->ctxt->regs_C.ftw, j))
       memcpy(&regs->regs_F.e[j], &handshake->ctxt->regs_F.e[j], MD_FPR_SIZE);

   if(!start_slice && core->fetch->PC != handshake->pc)
   {
     ZPIN_TRACE("PIN->PC (0x%x) different from fetch->PC (0x%x). Overwriting with Pin value!\n", handshake->pc, core->fetch->PC);
     info("PIN->PC (0x%x) different from fetch->PC (0x%x). Overwriting with Pin value!\n", handshake->pc, core->fetch->PC);
     cores[i]->fetch->PC = handshake->pc;
     regs->regs_PC = handshake->pc;
     regs->regs_NPC = handshake->pc;
   }
 
   bool fetch_more = true;
   consumed = false;
   bool repping = false;

   while(!consumed || repping || cores[i]->oracle->num_Mops_nuked > 0)
   {
     fetch_more = sim_main_slave_fetch_insn();
     fetches_since_feeder++;

     repping = thread->rep_sequence != 0;


     if(cores[i]->oracle->num_Mops_nuked > 0)
     {
       while(fetch_more && cores[i]->oracle->num_Mops_nuked > 0 &&
             !cores[i]->oracle->spec_mode)
       {       
         fetch_more = sim_main_slave_fetch_insn();
         fetches_since_feeder++;

         //Fetch can get more insns this cycle, but they are needed from PIN
         if(fetch_more && cores[i]->oracle->num_Mops_nuked == 0
                       && !cores[i]->oracle->spec_mode
                       && cores[i]->fetch->PC == NPC
                       && cores[i]->fetch->PC == regs->regs_NPC)
         {
            zesto_assert(cores[i]->fetch->PC == NPC, (void)0);
            return;
         }
       }

       //Fetch can get more insns this cycle, but not on nuke path 
       if(fetch_more)
       {
          consumed = false;
          continue;
       }
      
       sim_main_slave_post_pin();

       sim_main_slave_pre_pin();
       fetch_more = true;

       if(cores[i]->oracle->num_Mops_nuked == 0)
       {
         //Nuke recovery instruction is a mispredicted branch or REP-ed
         if(cores[i]->fetch->PC != NPC || regs->regs_NPC != NPC)
         {
            consumed = false;
            continue;
         }
         else //fetching from the correct addres, go back to Pin for instruction
         {
            zesto_assert(cores[i]->fetch->PC == NPC, (void)0);
            return;
         }
       }

     }
     /*XXX: here oracle still doesn't know if we're speculating or not. But if we predicted 
     the wrong path, we'd better not return to Pin, because that will mess the state up */
     else if((!repping && (cores[i]->fetch->PC != NPC || cores[i]->oracle->spec_mode)) ||
               //&& cores[i]->fetch->PC != handshake->pc) || //Not trapped
              repping) 
     {
       bool spec = false;
       do
       {
         while(fetch_more) 
         {
            fetch_more = sim_main_slave_fetch_insn();
            fetches_since_feeder++;

            spec = (cores[i]->oracle->spec_mode || (cores[i]->fetch->PC != regs->regs_NPC));
            /* If fetch can tolerate more insns, but needs to get them from PIN (f.e. after finishing a REP-ed instruction) */
            if(fetch_more && !spec && thread->rep_sequence == 0 && core->fetch->PC != handshake->pc && cores[i]->oracle->num_Mops_nuked == 0)
            {
               zesto_assert(cores[i]->fetch->PC == NPC, (void)0);
               return;
            }
         }
        
         sim_main_slave_post_pin();

         /* Next cycle */ 
         sim_main_slave_pre_pin();

         if(!consumed)
         {
            fetch_more = true;
            continue;
         }

         /* Potentially different after exec (in pre_pin) where branches are resolved */
         spec = (cores[i]->oracle->spec_mode || (cores[i]->fetch->PC != regs->regs_NPC));

         /* After recovering from spec and/or REP, we find no nukes -> great, get control back to PIN */
         if(thread->rep_sequence == 0 && core->fetch->PC != handshake->pc && !spec && cores[i]->oracle->num_Mops_nuked == 0)
         {
            zesto_assert(cores[i]->fetch->PC == NPC, (void)0);
            return;
         }

         /* After recovering from spec and/or REP, nuke -> go to nuke recovery loop */
         if(thread->rep_sequence == 0 && !spec && cores[i]->oracle->num_Mops_nuked > 0)
        {
            ZPIN_TRACE("Going from spec loop to nuke loop. PC: %x\n",cores[i]->fetch->PC);
            break;
         }

         /* All other cases should stay in this loop until they get resolved */
         fetch_more = true;

       }while(spec || thread->rep_sequence != 0);

     }
     else
     /* non-speculative, non-REP, non-nuke */
     {
       /* Pass control back to Pin to get a new PC on the same cycle*/
       if(fetch_more)
       {
          zesto_assert(cores[i]->fetch->PC == NPC, (void)0);
          return;
       }
    
       sim_main_slave_post_pin();

       /* This is already next cycle, up to fetch */
       sim_main_slave_pre_pin();
     }
   }

   zesto_assert(cores[i]->fetch->PC == NPC, (void)0);
}

void Zesto_Drain()
{
   assert(num_threads == 1);

   int i = 0;
   struct core_t * core = cores[i];

   /* Just flush anything left */
   core->oracle->complete_flush();
   core->commit->recover();
   core->exec->recover();
   core->alloc->recover();
   core->decode->recover();
   core->fetch->recover(core->current_thread->regs.regs_NPC);

   /* Invoke stages after fetch until all fetched insns get commited */
//   while(core->current_thread->stat.num_insn > core->stat.commit_insn)
//   while(core->oracle->get_oldest_Mop() != NULL)
//   {
//      sim_main_slave_pre_pin();
//   }
}
