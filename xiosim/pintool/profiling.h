#ifndef __FEEDER_PROFILING_H__
#define __FEEDER_PROFILING_H__

/* Check if instruction at @pc has a profiling analysis routine registered. */
bool HasProfilingInstrumentation(ADDRINT pc);

/* Add profiling routines for @img. */
void AddProfilingCallbacks(IMG img);

#endif /* __FEEDER_PROFILING_H__ */
