#ifndef ARCH_CPU_INCLUDED
#define ARCH_CPU_INCLUDED

#include "machine.h"
#include "memory.h"
#include "regs.h"

struct thread_t {

  int id;                    /* unique ID for each thread */

  struct regs_t regs;        /* simulated registers */

  int rep_sequence;          /* for implementing REP instructions */
  int asid;                  /* Address space ID that this thread belongs to. */

  char* rand_statebuf;       /* for per thread random number generation */
  struct random_data* rand_state;

  struct {
    counter_t num_insn;
    counter_t num_refs;
    counter_t num_loads;
    counter_t num_branches;
  } stat;

  bool active; /* FALSE if this core is not executing */
  bool finished_cycle;      /* Ready to advance to next cycle? */
  bool consumed;            /* Did fetching get an instruction back? */
  long long fetches_since_feeder; /* Instructions since last pin call */
  bool in_critical_section; /* Are we executing a HELIX sequential cut? */
  tick_t last_active_cycle; /* Last time this core was active */
};

/* architected state for each simulated thread/process */
extern struct thread_t ** threads;
extern int num_cores;

#endif /* ARCH_CPU_INCLUDED */
