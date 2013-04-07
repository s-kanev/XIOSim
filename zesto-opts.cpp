/* zesto-opts.c - Zesto command-line options/knobs
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
 */

#include "thread.h"
#include "loader.h"

#include "zesto-opts.h"
#include "zesto-core.h"
#include "zesto-oracle.h"
#include "zesto-fetch.h"
#include "zesto-decode.h"
#include "zesto-alloc.h"
#include "zesto-exec.h"
#include "zesto-cache.h"
#include "zesto-commit.h"
#include "zesto-dram.h"
#include "zesto-uncore.h"
#include "zesto-repeater.h"

#if defined(ZTRACE) && !defined(ZESTO_PIN_DBG)
FILE * ztrace_fp = NULL;
const char * ztrace_filename = NULL;
#endif

/* variables for fast-forwarding prior to detailed simulation */
long long fastfwd = 0;
long long trace_limit = 0; /* maximum number of instructions per trace - used for looping */

/* maximum number of inst's/uop's to execute */
long long max_insts = 0;
long long max_uops = 0;
long long max_cycles = 0;
int heartbeat_frequency = 0;

static bool ignored_flag = 0;
extern struct core_knobs_t knobs;

counter_t total_commit_insn = 0;
counter_t total_commit_uops = 0;
counter_t total_commit_eff_uops = 0;

/* register simulator-specific options */
void
sim_reg_options(struct opt_odb_t *odb)
{
  opt_reg_header(odb, 
      "sim-zesto: This simulator implements an execute-at-fetch timing\n"
      "simulator for x86 only.  Exec-at-exec is planned for the future.\n"
      );

  /* ignored flag used to terminate list options */
  opt_reg_flag(odb, "-","ignored flag",
      &ignored_flag, /*default*/ false, /*print*/false,/*format*/NULL);

  opt_reg_long_long(odb, "-fastfwd", "number of inst's to skip before timing simulation",
      &fastfwd, /* default */0, /* print */true, /* format */NULL);

  opt_reg_long_long(odb, "-tracelimit", "maximum number of instructions per trace",
      &trace_limit, /* default */0, /* print */true, /* format */NULL);

  opt_reg_int(odb, "-cores", "number of cores (if > 1, must provide this many eio traces)",
      &num_cores, /* default */1, /* print */true, /* format */NULL);
  opt_reg_flag(odb, "-multi_threaded", "do cores share an address space",
      &multi_threaded, /*default */false, /*print */true, /*format*/NULL);

  /* instruction limit */
  opt_reg_long_long(odb, "-max:inst", "maximum number of inst's to execute",
      &max_insts, /* default */0,
      /* print */true, /* format */NULL);
  opt_reg_long_long(odb, "-max:uops", "maximum number of uop's to execute",
      &max_uops, /* default */0,
      /* print */true, /* format */NULL);
  opt_reg_long_long(odb, "-max:cycles", "maximum number of cycles to execute",
      &max_cycles, /* default */0,

      /* print */true, /* format */NULL);
  opt_reg_int(odb, "-heartbeat", "frequency for which to print out simulator heartbeat",
      &heartbeat_frequency, /* default */0,
      /* print */true, /* format */NULL);

  opt_reg_string(odb, "-model","pipeline model type",
      &knobs.model, /*default*/ "DPM", /*print*/true,/*format*/NULL);

  opt_reg_int(odb, "-syscall:mem_lat", "number of cycles per syscall memory request (0 to omit syscall memory activity)",
      &knobs.memory.syscall_memory_latency, /* default */1, /* print */true, /* format */NULL);

#if defined(ZTRACE) && !defined(ZESTO_PIN_DBG)
  opt_reg_string(odb, "-ztrace:filename","zesto-trace filename",
      &ztrace_filename, /*default*/ NULL, /*print*/true,/*format*/NULL);
#endif

  opt_reg_flag(odb, "-power", "simulate power",
      &knobs.power.compute, /*default*/ false, /*print*/true,/*format*/NULL);

  opt_reg_int(odb, "-power:rtp_interval", "number of cycles between power computation",
      &knobs.power.rtp_interval, /* default */0, /* print */true, /* format */NULL);

  opt_reg_string(odb, "-power:rtp_file", "file to store runtime power trace",
      &knobs.power.rtp_filename, /* default */NULL, /* print */true, /* format */NULL);

  fetch_reg_options(odb,&knobs);
  decode_reg_options(odb,&knobs);
  alloc_reg_options(odb,&knobs);
  exec_reg_options(odb,&knobs);
  commit_reg_options(odb,&knobs);

  uncore_reg_options(odb);
  dram_reg_options(odb);
  repeater_reg_options(odb);

  opt_reg_string(odb, "-dvfs", "dvfs controller configuration string",
      &knobs.dvfs_opt_str, /* default */"none", /* print */true, /* format */NULL);

}

/* check simulator-specific option values */
void
sim_check_options(struct opt_odb_t *odb, int argc, char **argv)
{
  if(max_uops && max_uops < max_insts)
    warn("-max:uops is less than -max:inst, so -max:inst is useless");

  if((max_uops || max_insts) && max_cycles)
    warn("instruction/uop limit and cycle limit defined; will exit on whichever occurs first");

  if((num_cores < 1) || (num_cores > MAX_CORES))
    fatal("-cores must be between 1 and %d (inclusive)",MAX_CORES);

  simulated_processes_remaining = num_cores;

  uncore_create();
  dram_create();

#if defined(ZTRACE) && !defined(ZESTO_PIN_DBG)
  if(ztrace_filename && strcmp(ztrace_filename,""))
  {
    ztrace_fp = fopen(ztrace_filename,"w");
    if(!ztrace_fp)
      fatal("failed to open ztrace file %s",ztrace_filename);
  }
  else
    ztrace_fp = NULL;
#endif
}

/* register per-arch statistics */
void
cpu_reg_stats(struct core_t * core, struct stat_sdb_t *sdb)
{
  struct thread_t * arch = core->current_thread;

  core->oracle->reg_stats(sdb);
  core->fetch->reg_stats(sdb);
  core->decode->reg_stats(sdb);
  core->alloc->reg_stats(sdb);
  core->exec->reg_stats(sdb);
  core->commit->reg_stats(sdb);
  ld_reg_stats(core->current_thread,sdb);

  /* only print this out once at the very end */
  if(core->current_thread->id == (num_cores-1))
  {
    uncore_reg_stats(sdb);
    mem_reg_stats(arch->mem, sdb);
  }
}

/* register simulator-specific statistics */
void
sim_reg_stats(struct thread_t ** archs, struct stat_sdb_t *sdb)
{
  int i;
  char buf[1024];
  char buf2[1024];
  bool is_DPM = strcasecmp(knobs.model,"DPM") == 0;

  /* per core stats */
  for(i=0;i<num_cores;i++)
    cpu_reg_stats(cores[i],sdb);

  stat_reg_note(sdb,"\n#### SIMULATOR PERFORMANCE STATS ####");
  stat_reg_qword(sdb, true, "sim_cycle", "total simulation cycles (CPU cycles assuming default freq)", (qword_t*)&uncore->default_cpu_cycles, 0, TRUE, NULL);
  stat_reg_int(sdb, true, "sim_elapsed_time", "total simulation time in seconds", &sim_elapsed_time, 0, TRUE, NULL);
  stat_reg_formula(sdb, true, "sim_cycle_rate", "simulation speed (in Mcycles/sec)", "sim_cycle / (sim_elapsed_time * 1000000.0)", NULL);
  /* Make formula to add num_insn from all archs */
  strcpy(buf2,"");
  for(i=0;i<num_cores;i++)
  {
    if(i==0)
      sprintf(buf,"c%d.commit_insn",i);
    else
      sprintf(buf," + c%d.commit_insn",i);

    strcat(buf2,buf);
  }
  stat_reg_formula(sdb, true, "all_insn", "total insts simulated for all cores", buf2, "%12.0f");
  stat_reg_formula(sdb, true, "sim_inst_rate", "simulation speed (in MIPS)", "all_insn / (sim_elapsed_time * 1000000.0)", NULL);

  /* Make formula to add num_uops from all archs */
  strcpy(buf2,"");
  for(i=0;i<num_cores;i++)
  {
    if(i==0)
      sprintf(buf,"c%d.commit_uops",i);
    else
      sprintf(buf," + c%d.commit_uops",i);

    strcat(buf2,buf);
  }
  stat_reg_formula(sdb, true, "all_uops", "total uops simulated for all cores", buf2, "%12.0f");
  stat_reg_formula(sdb, true, "sim_uop_rate", "simulation speed (in MuPS)", "all_uops / (sim_elapsed_time * 1000000.0)", NULL);

  /* Make formula to add num_eff_uops from all archs */
  if(is_DPM)
  {
    strcpy(buf2,"");
    for(i=0;i<num_cores;i++)
    {
      if(i==0)
        sprintf(buf,"c%d.commit_eff_uops",i);
      else
        sprintf(buf," + c%d.commit_eff_uops",i);

      strcat(buf2,buf);
    }
    stat_reg_formula(sdb, true, "all_eff_uops", "total effective uops simulated for all cores", buf2, "%12.0f");
    stat_reg_formula(sdb, true, "sim_eff_uop_rate", "simulation speed (in MeuPS)", "all_eff_uops / (sim_elapsed_time * 1000000.0)", NULL);
  }

  if(num_cores == 1) /* single-thread */
  {
    sprintf(buf,"c0.commit_IPC");
    stat_reg_formula(sdb, true, "total_IPC", "final commit IPC", buf, NULL);
  }
  else
  {
    /* Harmonic Means - Note only HM_IPC really makes sense when you use
       -max:inst NNN because the harmonic mean depends on all cores executing
       the same number of total instructions.  If you use -max:uops, then
       HM_uPC makes sense. */
    strcpy(buf2,"");
    for(i=0;i<num_cores;i++)
    {
      if(i==0)
        sprintf(buf,"%d.0 / ( (1.0/c%d.commit_IPC)",num_cores,i);
      else
        sprintf(buf," + (1.0/c%d.commit_IPC)",i);

      strcat(buf2,buf);
    }
    sprintf(buf," )");
    strcat(buf2,buf);
    stat_reg_formula(sdb, true, "HM_IPC", "harmonic mean IPC across all cores", buf2, NULL);

    strcpy(buf2,"");
    for(i=0;i<num_cores;i++)
    {
      if(i==0)
        sprintf(buf,"%d.0 / ( (1.0/c%d.commit_uPC)",num_cores,i);
      else
        sprintf(buf," + (1.0/c%d.commit_uPC)",i);

      strcat(buf2,buf);
    }
    sprintf(buf," )");
    strcat(buf2,buf);
    stat_reg_formula(sdb, true, "HM_uPC", "harmonic mean uPC across all cores", buf2, NULL);

    if(is_DPM)
    {
      strcpy(buf2,"");
      for(i=0;i<num_cores;i++)
      {
        if(i==0)
          sprintf(buf,"%d.0 / ( (1.0/c%d.commit_euPC)",num_cores,i);
        else
          sprintf(buf," + (1.0/c%d.commit_euPC)",i);

        strcat(buf2,buf);
      }
      sprintf(buf," )");
      strcat(buf2,buf);
      stat_reg_formula(sdb, true, "HM_euPC", "harmonic mean euPC across all cores", buf2, NULL);
    }

    /* Geometric Means */
    strcpy(buf2,"^((");
    for(i=0;i<num_cores;i++)
    {
      if(i==0)
        sprintf(buf,"(!c%d.commit_IPC)",i);
      else
        sprintf(buf," + (!c%d.commit_IPC)",i);

      strcat(buf2,buf);
    }
    sprintf(buf," )/%d.0)",num_cores);
    strcat(buf2,buf);
    stat_reg_formula(sdb, true, "GM_IPC", "geometric mean IPC across all cores", buf2, NULL);

    strcpy(buf2,"^((");
    for(i=0;i<num_cores;i++)
    {
      if(i==0)
        sprintf(buf,"(!c%d.commit_uPC)",i);
      else
        sprintf(buf," + (!c%d.commit_uPC)",i);

      strcat(buf2,buf);
    }
    sprintf(buf," )/%d.0)",num_cores);
    strcat(buf2,buf);
    stat_reg_formula(sdb, true, "GM_uPC", "geometric mean uPC across all cores", buf2, NULL);

    if(is_DPM)
    {
      strcpy(buf2,"^((");
      for(i=0;i<num_cores;i++)
      {
        if(i==0)
          sprintf(buf,"(!c%d.commit_euPC)",i);
        else
          sprintf(buf," + (!c%d.commit_euPC)",i);

        strcat(buf2,buf);
      }
      sprintf(buf," )/%d.0)",num_cores);
      strcat(buf2,buf);
      stat_reg_formula(sdb, true, "GM_euPC", "geometric mean euPC across all cores", buf2, NULL);
    }

    /* The following stats are statistically invalid.  You can't add IPC's because
       in each case, the denominators (cycles per benchmark) are different, and so
       the sum of the IPCs (or uPCs) has no physical meaning. */
    strcpy(buf2,"");
    for(i=0;i<num_cores;i++)
    {
      if(i==0)
        sprintf(buf,"c%d.commit_IPC",i);
      else
        sprintf(buf," + c%d.commit_IPC",i);

      strcat(buf2,buf);
    }
    stat_reg_formula(sdb, true, "TP_IPC", "IPC ThroughPut for all cores (this is a nonsense metric: you can't add IPCs)", buf2, NULL);

    strcpy(buf2,"");
    for(i=0;i<num_cores;i++)
    {
      if(i==0)
        sprintf(buf,"c%d.commit_uPC",i);
      else
        sprintf(buf," + c%d.commit_uPC",i);

      strcat(buf2,buf);
    }
    stat_reg_formula(sdb, true, "TP_uPC", "uPC ThroughPut for all cores (this is a nonsense metric: you can't add uPCs)", buf2, NULL);

    if(is_DPM)
    {
      strcpy(buf2,"");
      for(i=0;i<num_cores;i++)
      {
        if(i==0)
          sprintf(buf,"c%d.commit_euPC",i);
        else
          sprintf(buf," + c%d.commit_euPC",i);

        strcat(buf2,buf);
      }
      stat_reg_formula(sdb, true, "TP_euPC", "euPC ThroughPut for all cores (this is a nonsense metric: you can't add euPCs)", buf2, NULL);
    }

    stat_reg_counter(sdb, true, "total_insn", "total instructions simulated for all cores, including instructions from inactive (looping) cores", &total_commit_insn, 0, TRUE, NULL);
    stat_reg_counter(sdb, true, "total_uops", "total uops simulated for all cores, including uops from inactive (looping) cores", &total_commit_uops, 0, TRUE, NULL);
    if(is_DPM)
      stat_reg_counter(sdb, true, "total_eff_uops", "total effective uops simulated for all cores, including effective uops from inactive (looping) cores", &total_commit_eff_uops, 0, TRUE, NULL);

    /* The following IPC stats are fine.  We can in fact add up these IPCs because in each case,
       the number of cycles is the same; however, you probably can't compare this number against
       anything, because for other simulation runs, the number of cycles will likely be different.
       This still gives you a ball-park idea of how much throughput you're getting through the
       machine overall. */
    stat_reg_formula(sdb, true, "total_IPC", "total IPC of all cores, including instructions from inactive (looping) cores", "total_insn / sim_cycle", NULL);
    stat_reg_formula(sdb, true, "total_uPC", "total uPC of all cores, including instructions from inactive (looping) cores", "total_uops / sim_cycle", NULL);
    if(is_DPM)
      stat_reg_formula(sdb, true, "total_euPC", "total euPC of all cores, including instructions from inactive (looping) cores", "total_eff_uops / sim_cycle", NULL);

    stat_reg_formula(sdb, true, "total_inst_rate", "simulation speed (in MIPS)", "total_insn / (sim_elapsed_time * 1000000.0)", NULL);
    stat_reg_formula(sdb, true, "total_uop_rate", "simulation speed (in MuPS)", "total_uops / (sim_elapsed_time * 1000000.0)", NULL);
    if(is_DPM)
      stat_reg_formula(sdb, true, "total_eff_uop_rate", "simulation speed (in MeuPS)", "total_eff_uops / (sim_elapsed_time * 1000000.0)", NULL);

    stat_reg_formula(sdb, true, "loop_inst_overhead", "overhead rate for additional looping instructions", "total_insn / all_insn", NULL);
    stat_reg_formula(sdb, true, "loop_uop_overhead", "overhead rate for additional looping uops", "total_uops / all_uops", NULL);
    if(is_DPM)
      stat_reg_formula(sdb, true, "loop_eff_uop_overhead", "overhead rate for additional looping effective uops", "total_eff_uops / all_eff_uops", NULL);
  }

}

#ifdef ZTRACE

#ifdef ZESTO_PIN_DBG
#define ZTRACE_PRINT(fmt, ...) ZPIN_TRACE(fmt, ## __VA_ARGS__)
#define ZTRACE_VPRINT(fmt, v) vtrace(fmt, v)
#define ZTRACE_CHECK_FILE()
#else
#define ZTRACE_PRINT(fmt, ...)  fprintf(ztrace_fp, fmt, ## __VA_ARGS__)
#define ZTRACE_VPRINT(fmt, v)   myvfprintf(ztrace_fp, fmt, v)
#define ZTRACE_CHECK_FILE() \
if(ztrace_fp == NULL) \
return;
#endif /* ZESTO_PIN_DBG */

void ztrace_Mop_ID(const struct Mop_t * Mop)
{
  ZTRACE_PRINT("%lld|M:%lld|",Mop->core->sim_cycle,Mop->oracle.seq);
  if(Mop->oracle.spec_mode)
    ZTRACE_PRINT("X|");
  else
    ZTRACE_PRINT(".|");
}

void ztrace_uop_ID(const struct uop_t * uop)
{
  if(uop==NULL)
    return;
  ZTRACE_PRINT("%lld|u:%lld:%lld|", uop->core->sim_cycle,
          uop->decode.Mop_seq, (uop->decode.Mop_seq << UOP_SEQ_SHIFT) + uop->flow_index);
  if(uop->Mop && uop->Mop->oracle.spec_mode)
    ZTRACE_PRINT("X|");
  else
    ZTRACE_PRINT(".|");
}


/* called by oracle when Mop first executes */
void ztrace_print(const struct Mop_t * Mop)
{
  ZTRACE_CHECK_FILE();

  ztrace_Mop_ID(Mop);
  // core id, PC{virtual,physical}
  ZTRACE_PRINT("DEF|core=%d:virtPC=%x:physPC=%llx:op=",Mop->core->id,Mop->fetch.PC,v2p_translate(Mop->core->id,Mop->fetch.PC));
  // rep prefix and iteration
  if(Mop->fetch.inst.rep)
    ZTRACE_PRINT("rep{%d}",Mop->decode.rep_seq);
  // opcode name
  ZTRACE_PRINT("%s:",md_op2name[Mop->decode.op]);
  ZTRACE_PRINT("trap=%d:",Mop->decode.is_trap);

  // ucode flow length
  ZTRACE_PRINT("flow-length=%d\n",Mop->decode.flow_length);

  int i;
  int count=0;
  for(i=0;i<Mop->decode.flow_length;)
  {
    struct uop_t * uop = &Mop->uop[i];
    ztrace_uop_ID(uop);
    ZTRACE_PRINT("DEF");
    if(uop->decode.BOM && !uop->decode.EOM) ZTRACE_PRINT("-BOM");
    if(uop->decode.EOM && !uop->decode.BOM) ZTRACE_PRINT("-EOM");
    // core id, uop number within flow
    ZTRACE_PRINT("|core=%d:uop-number=%d:",Mop->core->id,count);
    // opcode name
    ZTRACE_PRINT("op=%s",md_op2name[uop->decode.op]);
    if(uop->decode.in_fusion)
    {
      ZTRACE_PRINT("-f");
      if(uop->decode.is_fusion_head)
        ZTRACE_PRINT("H"); // fusion head
      else
        ZTRACE_PRINT("b"); // fusion body
    }

    // register identifiers
    ZTRACE_PRINT(":odep=%d:i0=%d:i1=%d:i2=%d:",
        uop->decode.odep_name,
        uop->decode.idep_name[0],
        uop->decode.idep_name[1],
        uop->decode.idep_name[2]);

    // load/store address and size
    if(uop->decode.is_load || uop->decode.is_sta)
      ZTRACE_PRINT("VA=%lx:PA=%llx:mem-size=%d:fault=%d",(long unsigned int)uop->oracle.virt_addr,uop->oracle.phys_addr,uop->decode.mem_size,uop->oracle.fault);

    ZTRACE_PRINT("\n");

    i += Mop->uop[i].decode.has_imm?3:1;
    count++;
  }
#ifndef ZESTO_PIN_DBG
  fflush(ztrace_fp);
#endif
}

void ztrace_print(const struct Mop_t * Mop, const char * fmt, ... )
{
  ZTRACE_CHECK_FILE();
  va_list v;
  va_start(v, fmt);

  ztrace_Mop_ID(Mop);
  ZTRACE_VPRINT( fmt, v);
  ZTRACE_PRINT("\n");
}

void ztrace_print(const struct uop_t * uop, const char * fmt, ... )
{
  ZTRACE_CHECK_FILE();
  va_list v;
  va_start(v, fmt);

  ztrace_uop_ID(uop);
  ZTRACE_VPRINT( fmt, v);
  ZTRACE_PRINT("\n");
}

void ztrace_print(const char * fmt, ... )
{
  ZTRACE_CHECK_FILE();
  va_list v;
  va_start(v, fmt);

  ZTRACE_VPRINT( fmt, v);
  ZTRACE_PRINT("\n");
}

void ztrace_print_start(const struct uop_t * uop, const char * fmt, ... )
{
  ZTRACE_CHECK_FILE();
  va_list v;
  va_start(v, fmt);

  ztrace_uop_ID(uop);
  ZTRACE_VPRINT( fmt, v);
}

void ztrace_print_cont(const char * fmt, ... )
{
  ZTRACE_CHECK_FILE();
  va_list v;
  va_start(v, fmt);

  ZTRACE_VPRINT( fmt, v);
}

void ztrace_print_finish(const char * fmt, ... )
{
  ZTRACE_CHECK_FILE();
  va_list v;
  va_start(v, fmt);

  ZTRACE_VPRINT( fmt, v);
  ZTRACE_PRINT("\n");
}

#endif
