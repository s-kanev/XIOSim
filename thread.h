#ifndef ARCH_CPU_INCLUDED
#define ARCH_CPU_INCLUDED

#include "machine.h"
#include "memory.h"
#include "regs.h"

struct thread_t {

  int id;                    /* unique ID for each thread */

  struct mem_t *mem;         /* simulated virtual memory */

  struct regs_t regs;        /* simulated registers */

  int rep_sequence;          /* for implementing REP instructions */

  struct {
    md_addr_t brk_point;     /* top of the data segment */
    md_addr_t stack_base;    /* program stack segment base (highest address in stack) */
    md_addr_t stack_min;     /* lowest address accessed on the stack */
  } memory;

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

#endif /* ARCH_CPU_INCLUDED */
