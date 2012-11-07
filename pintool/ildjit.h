#ifndef __MOLECOOL_PIN__
#define __MOLECOOL_PIN__

/*
 * ILDJIT-specific functions for zesto feeder
 * Copyright, Svilen Kanev, 2012
 */

VOID MOLECOOL_Init();
VOID AddILDJITCallbacks(IMG img);
BOOL ILDJIT_IsExecuting();
BOOL ILDJIT_IsCreatingExecutor();
VOID printMemoryUsage(THREADID tid);

class loop_state_t
{
 public:
  int simmed_iteration_count;
  bool use_ring_cache;
  ADDRINT current_loop;
};

#endif /* __MOLECOOL_PIN__ */
