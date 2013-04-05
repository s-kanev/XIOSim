#ifndef ARCH_CPU_INCLUDED
#define ARCH_CPU_INCLUDED

#include "machine.h"
#include "memory.h"
#include "regs.h"

struct thread_t {

  int id;                    /* unique ID for each thread */

  int current_core;          /* id of current core this thread is running on */

  struct mem_t *mem;         /* simulated virtual memory */

  struct regs_t regs;        /* simulated registers */

  md_addr_t * ldt_p;

  int rep_sequence;          /* for implementing REP instructions */

  struct {
    md_addr_t text_base;     /* program text (code) segment base */
    md_addr_t text_bound;    /* program text (code) segment bound */
    unsigned int text_size;  /* program text (code) size in bytes */
    md_addr_t data_base;     /* program initialized data segment base */
    md_addr_t data_bound;    /* program initialized data segment bound */
    md_addr_t brk_point;     /* top of the data segment */
    unsigned int data_size;  /* program initialized ".data" and uninitialized ".bss" size in bytes */
    md_addr_t stack_base;    /* program stack segment base (highest address in stack) */
    unsigned int stack_size; /* program initial stack size */
    md_addr_t stack_min;     /* lowest address accessed on the stack */
    char *prog_fname;        /* program file name */
    md_addr_t prog_entry;    /* program entry point (initial PC) */
    md_addr_t environ_base;  /* program environment base address address */
    int target_big_endian;   /* target executable endian-ness, non-zero if big endian */
  } loader;

  struct {
    counter_t num_insn;
    counter_t num_refs;
    counter_t num_loads;
    counter_t num_branches;
  } stat;

  bool active; /* FALSE if this core is not executing */
#ifdef ZESTO_PIN
  bool finished_cycle;      /* Ready to advance to next cycle? */
  bool consumed;            /* Did fetching get an instruction back? */
  bool first_insn;          /* Excuted at least an istruction? */
  long long fetches_since_feeder; /* Instructions since last pin call */
  bool in_critical_section; /* Are we executing a HELIX sequential cut? */
  tick_t last_active_cycle; /* Last time this core was active */
#endif
};

/* architected state for each simulated thread/process */
extern struct thread_t ** threads;
extern int num_cores;
extern bool multi_threaded;
extern int simulated_processes_remaining;

/* load program into simulated state; returns 1 if program is an eio trace */
int sim_load_prog(struct thread_t * thread,  /* target thread */
                   char *fname,              /* program to load */
                   int argc, char **argv,    /* program arguments */
                   char **envp               /* program environment */
                  );

/* print simulator-specific configuration information */
void sim_aux_config(FILE *stream);

/* dump simulator-specific auxiliary simulator statistics */
void sim_aux_stats(FILE *stream);

/* un-initialize simulator-specific state */
void sim_uninit(void);

#endif /* ARCH_CPU_INCLUDED */
