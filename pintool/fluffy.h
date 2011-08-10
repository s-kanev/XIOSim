#ifndef __FLUFFY_MOLECOOL__
#define __FLUFFY_MOLECOOL__

/* 
 * Fluffy - looking for regions of interest when running under ILDJIT.
 * Copyright, Svilen Kanev, 2011
 */

VOID FLUFFY_Init();
VOID FLUFFY_StartInsn(THREADID tid, ADDRINT pc, ADDRINT phase);
VOID FLUFFY_StopInsn(THREADID tid, ADDRINT pc, ADDRINT phase);

#endif /*__FLUFFY_MOLECOOL__ */
