#ifndef __ZTRACE_H__
#define __ZTRACE_H__

/* Ztrace keeps a circular trace buffer of the last several thousand microarchitectural
 * events. It is extremeley useful when debugging simulator bugs that happen further
 * along in the execution, when the full trace won't fit on hard drives.
 * There is one circular buffer per core and one for the uncore.
 */

/* Prefix for ztrace output files.
 * Final filenames will be @ztrace_filename.{coreID}.
 */
#include "stats.h"
#include "thread.h"
#include "zesto-core.h"

extern const char * ztrace_filename;

#ifdef ZTRACE
/* Opens ztrace files */
extern void ztrace_init();
/* Flushes the in-memory traces to files */
extern void ztrace_flush();

/* Generic tracing functions.
 * @coreID == INVALID_CORE prints to the uncore trace.
 */
extern void vtrace(const int coreID, const char *fmt, va_list v);
extern void trace(const int coreID, const char *fmt, ...)
    __attribute__ ((format (printf, 2, 3)));

#define ZTRACE_PRINT(coreID, fmt, ...) trace(coreID, fmt, ## __VA_ARGS__)
#define ZTRACE_VPRINT(coreID, fmt, v) vtrace(coreID, fmt, v)

/* Convenience functions for tracing Mops and uops respectively. */
void ztrace_print(const struct Mop_t * Mop);
void ztrace_Mop_timing(const struct Mop_t * Mop);
void ztrace_print(const struct Mop_t * Mop, const char * fmt, ... );
void ztrace_print(const struct uop_t * uop, const char * fmt, ... );
void ztrace_print(const int coreID, const char * fmt, ... );

void ztrace_print_start(const struct uop_t * uop, const char * fmt, ... );
void ztrace_print_cont(const int coreID, const char * fmt, ... );
void ztrace_print_finish(const int coreID, const char * fmt, ... );

void ztrace_uop_ID(const struct uop_t * uop);
void ztrace_uop_alloc(const struct uop_t * uop);
void ztrace_uop_timing(const struct uop_t * uop);

#else

inline void ztrace_init() {}
/* Assert macros rely that this is defined. */
inline void ztrace_flush() {}

#define ZTRACE_PRINT(coreID, fmt, ...)
#define ZTRACE_VPRINT(coreID, fmt, v)

#endif

#endif /* __ZTRACE_H__ */
