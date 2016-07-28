#ifndef __TCM_HOOKS_H__
#define __TCM_HOOKS_H__

#include "xiosim/size_class_cache.h"

void InstrumentTCMHooks(TRACE trace, VOID* v);
void InstrumentTCMIMGHooks(IMG img);

#endif  /* __TCM_HOOKS_H__ */
