/* 
 * Callbacks to instruction feeder.
 * Copyright, Svilen Kanev, 2011
*/

#ifndef __PIN_ZESTO_CALLBACKS__
#define __PIN_ZESTO_CALLBACKS__

/* Called on every write to program memory */
extern "C" void Zesto_Call_WriteByteCallback(unsigned int addr, unsigned char val_to_write);

/* Called once a core has consumed the handshake and it can be reused */
extern void ReleaseHandshake(unsigned int instrument_tid);

#endif /* __PIN_ZESTO_CALLBACKS__ */
