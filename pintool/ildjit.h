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

#endif /* __MOLECOOL_PIN__ */
