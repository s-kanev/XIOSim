/*
 * Scheduling interface between app threads and cores
 * Copyright, Svilen Kanev, 2013
 */

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

namespace xiosim {

const pid_t INVALID_THREADID = -1;

void InitScheduler(int num_cores);

/* Notify the scheduler a new threads is created.
 * It will be immediately marked for scheduling on a core.
 */
void ScheduleNewThread(pid_t tid);

/* Notify the scheduler that thread @tid will only
 * run on core @coreID.
 */
void SetThreadAffinity(pid_t tid, int coreID);

/* Get the current running thread on core @coreID.
 * returns INVALID_THREADID if core is not active.
 */
pid_t GetCoreThread(int coreID);

/* Remove a thread from the scheduler once it exits,
 * and we are done simulating it. This will free the
 * thread state and deactivate the core, if needed.
 */
void DescheduleActiveThread(int coreID);

/* Let another thread (if any) scheduled on core
 * @coreID take over.
 * Assumes it is only called from the core sim thread */
void GiveUpCore(int coreID, bool reschedule_thread);

/* A check whether anything is running on core
 * @coreID. This serves the same purpose as is_core_active(),
 * but is thread-safe and can be called from any thread.
 */
bool IsCoreBusy(int coreID);

/* Ask scheduler if it's time give up core @coreID.
 * Assumes it is only called from the core sim thread
 */
bool NeedsReschedule(int coreID);

} // namespace xiosim

#endif /* __SCHEDULER_H__ */
