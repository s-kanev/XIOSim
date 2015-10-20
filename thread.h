#ifndef __THREAD_H__
#define __THREAD_H__

#include "host.h"

struct thread_t {

  int id;                    /* unique ID for each thread */
  int asid;                  /* Address space ID that this thread belongs to. */

  struct {
    counter_t num_insn;
    counter_t num_refs;
    counter_t num_loads;
    counter_t num_branches;
  } stat;

  bool finished_cycle;      /* Ready to advance to next cycle? */
  bool consumed;            /* Did fetching get an instruction back? */
  bool in_critical_section; /* Are we executing a HELIX sequential cut? */
};

#endif /* __THREAD_H__ */
