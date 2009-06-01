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
#include "loader.h"
#include "sim.h"

/* stats signal handler */
static void
signal_sim_stats(int sigtype)
{
  sim_dump_stats = TRUE;
}

/* exit signal handler */
static void
signal_exit_now(int sigtype)
{
  sim_exit_now = TRUE;
}

/* execution start/end times */
time_t sim_start_time;
time_t sim_end_time;
int sim_elapsed_time;

/* byte/word swapping required to execute target executable on this host */
int sim_swap_bytes;
int sim_swap_words;

/* exit when this becomes non-zero */
int sim_exit_now = FALSE;

/* longjmp here when simulation is completed */
jmp_buf sim_exit_buf;

/* set to non-zero when simulator should dump statistics */
int sim_dump_stats = FALSE;

/* options database */
struct opt_odb_t *sim_odb;

/* stats database */
struct stat_sdb_t *sim_sdb;

/* EIO interfaces */
char *sim_eio_fname[MAX_CORES];
FILE *sim_eio_fd[MAX_CORES];

/* redirected program/simulator output file names */
static char *sim_simout = NULL;
static char *sim_progout = NULL;
FILE *sim_progfd = NULL;

/* track first argument orphan, this is the program to execute */
static int exec_index = -1;

/* dump help information */
static bool help_me;

/* random number generator seed */
static int rand_seed;

/* initialize and quit immediately */
static bool init_quit;

/* simulator scheduling priority */
static int nice_priority;

/* default simulator scheduling priority */
#define NICE_DEFAULT_VALUE		0

static int
orphan_fn(int i, int argc, char **argv)
{
  exec_index = i;
  return /* done */FALSE;
}

static void
banner(FILE *fd, int argc, char **argv)
{
  char *s;

  fprintf(fd,
	  "%s: SimpleScalar/%s Tool Set version %d.%d of %s.\n"
	  "Copyright (C) 2000-2002 by The Regents of The University of Michigan.\n"
          "Copyright (C) 1994-2002 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.\n"
	  "This version of SimpleScalar is licensed for academic non-commercial use only.\n"
	  "\n",
	  ((s = strrchr(argv[0], '/')) ? s+1 : argv[0]),
	  VER_TARGET, VER_MAJOR, VER_MINOR, VER_UPDATE);
}

static void
usage(FILE *fd, int argc, char **argv)
{
  fprintf(fd, "Usage: %s {-options} executable {arguments}\n", argv[0]);
  opt_print_help(sim_odb, fd);
}

static int running = FALSE;

/* print all simulator stats */
void
sim_print_stats(FILE *fd)		/* output stream */
{
  if (!running)
    return;

  /* get stats time */
  sim_end_time = time((time_t *)NULL);
  sim_elapsed_time = MAX(sim_end_time - sim_start_time, 1);

  /* print simulation stats */
  fprintf(fd, "\nsim: ** simulation statistics **\n");
  stat_print_stats(sim_sdb, fd);
  sim_aux_stats(fd);
  fprintf(fd, "\n");
}

/* print stats, uninitialize simulator components, and exit w/ exitcode */
static void
exit_now(int exit_code)
{
  /* print simulation stats */
  sim_print_stats(stderr);

  /* un-initialize the simulator */
  sim_uninit();

  /* all done! */
  exit(exit_code);
}

int
main(int argc, char **argv, char **envp)
{
  char *s;
  int i, exit_code;

  /* catch SIGUSR1 and dump intermediate stats */
  signal(SIGUSR1, signal_sim_stats);

  /* catch SIGUSR2 and dump final stats and exit */
  signal(SIGUSR2, signal_exit_now);

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

  /* set simulator scheduling priority */
  if (nice(0) < nice_priority)
    {
      if (nice(nice_priority - nice(0)) < 0)
        fatal("could not renice simulator process");
    }

  /* initialize the instruction decoder */
  md_init_decoder();

  /* initialize all simulation modules */
  sim_post_init();

  /* initialize architected state */
  for(i=0;i<num_threads;i++)
  {
    if((num_threads > 1) && (exec_index == argc))
      fatal("if you set -cores to %d, you must provide %d EIO trace inputs",num_threads,num_threads);
    int is_eio = sim_load_prog(threads[i],argv[exec_index], argc-exec_index, argv+exec_index, envp);
    if(is_eio)
      exec_index ++;
    else if(num_threads > 1)
      fatal("only EIO traces supported in multi-core mode");
  }

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
  sim_main();

  /* simulation finished early */
  exit_now(0);

  return 0;
}
