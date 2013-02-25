/*
 * Scheduling interface between app threads and cores
 * Copyright, Svilen Kanev, 2013
 */

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

VOID InitScheduler(INT32 num_cores);

/* Notify the scheduler a new threads is created.
 * It will be immediately marked for scheduling on a core.
 */
VOID ScheduleNewThread(THREADID tid);

/* Get the current running thread on core @coreID.
 * returns INVALID_THREADID if core is not active.
 */
THREADID GetCoreThread(INT32 coreID);

/* Remove a thread from the scheduler once it exits,
 * and we are done simulating it. This will free the
 * thread state and deactivate the core, if needed.
 */
VOID DescheduleActiveThread(INT32 coreID);

/* Hook to not use the scheduler, if simulated
 * program (HELIX) is already taking care of it.
 */
VOID HardcodeSchedule(THREADID tid, INT32 coreID);

/* Let another thread (if any) scheduled on core
 * @coreID take over. */
VOID GiveUpCore(INT32 coreID, BOOL reschedule_thread);

/* A check whether anything is running on core
 * @coreID. This serves the same purpose as is_core_active(),
 * but is thread-safe and can be called from any thread.
 */
BOOL IsCoreBusy(INT32 coreID);

#endif /* __SCHEDULER_H__ */
