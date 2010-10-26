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

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "endian.h"
#include "version.h"
#include "options.h"
#include "stats.h"
#include "interface.h"
#include "loader.h"
#include "sim.h"

#include "zesto-opts.h"
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


extern int start_pos;
extern int heartbeat_count;

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
  knobs.model = "DPM";

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


//Returns true if another instruction can be fetched in the same cycle
bool sim_main_slave_fetch_insn()
{
  int i=0;

  assert(num_threads == 1);

  return cores[mod2m(start_pos+i,num_threads)]->fetch->do_fetch();
}

void sim_main_slave_post_pin()
{
  int i;

  for(i=0;i<num_threads;i++)
  {
    /* round-robin on which cache to process first so that one core
       doesn't get continual priority over the others for L2 access */
    cores[mod2m(start_pos+i,num_threads)]->fetch->pre_fetch();
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
      fatal("Core %d got hosed, quitting."); 
  }

  start_pos = modinc(start_pos,num_threads);

  if((heartbeat_frequency > 0) && (heartbeat_count >= heartbeat_frequency))
  {
    fprintf(stderr,"##HEARTBEAT## %lld: {",sim_cycle);
    for(i=0;i<num_threads;i++)
    {
      if(i < (num_threads-1))
	myfprintf(stderr,"%lld, ",cores[i]->stat.commit_insn);
      else
	myfprintf(stderr,"%lld}\n",cores[i]->stat.commit_insn);
    }
    heartbeat_count = 0;
  }


}

void sim_main_slave_pre_pin()
{
  int i;

  //for(i=0;i<num_threads;i++)
    //cores[i]->fetch->PC = cores[i]->current_thread->regs.regs_PC;

  ZPIN_TRACE("###Cycle%s\n"," ")

  if(sim_cycle == 0)
    myfprintf(stderr, "### starting timing simulation \n");

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

  for(i=0;i<num_threads;i++)
    cores[i]->commit->step();

  /* all memory processed here */
  for(i=0;i<num_threads;i++)
  {
    /* round-robin on which cache to process first so that one core
       doesn't get continual priority over the others for L2 access */
    cores[mod2m(start_pos+i,num_threads)]->exec->LDST_exec();
  }
  for(i=0;i<num_threads;i++)
    cores[i]->exec->ALU_exec();

  for(i=0;i<num_threads;i++)
    cores[i]->exec->LDQ_schedule();
  for(i=0;i<num_threads;i++)
    cores[i]->exec->RS_schedule();
  
  for(i=0;i<num_threads;i++)
    cores[i]->alloc->step();

  for(i=0;i<num_threads;i++)
    cores[i]->decode->step();

  for(i=0;i<num_threads;i++)
  {
    /* round-robin on which cache to process first so that one core
       doesn't get continual priority over the others for L2 access */
    cores[mod2m(start_pos+i,num_threads)]->fetch->post_fetch();
  }

}


void sim_main_slave_step()
{
   sim_main_slave_post_pin();
 
   while(sim_main_slave_fetch_insn());

   sim_main_slave_pre_pin();
}

