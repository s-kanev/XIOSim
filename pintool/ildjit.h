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
BOOL ILDJIT_GetExecutingTID();
BOOL ILDJIT_IsDoneFastForwarding();

VOID printMemoryUsage(THREADID tid);
/* Insert additional fake instructions to end the loop properly (flush RC, etc.). */
VOID InsertHELIXPauseCode(THREADID tid, bool first_thread);

class loop_state_t
{
 public:
  int simmed_iteration_count;
  bool use_ring_cache;
  ADDRINT current_loop;
  UINT32 invocationCount;
  UINT32 iterationCount;
};

#endif /* __MOLECOOL_PIN__ */
