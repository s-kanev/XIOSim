/* 
 * Callbacks to instruction feeder.
 * Copyright, Svilen Kanev, 2011
*/

#ifndef __PIN_ZESTO_CALLBACKS__
#define __PIN_ZESTO_CALLBACKS__

#include "interface.h"

extern "C" void Zesto_Call_WriteByteCallback(unsigned int addr, unsigned char val_to_write);

#endif /* __PIN_ZESTO_CALLBACKS__ */
