/* 
 * Callbacks to instruction feeder.
 * Copyright, Svilen Kanev, 2011
*/

#include "callbacks.h"

static ZESTO_WRITE_BYTE_CALLBACK write_byte_call = NULL;

void Zesto_Add_WriteByteCallback(ZESTO_WRITE_BYTE_CALLBACK callback)
{
    assert(write_byte_call == NULL);

    write_byte_call = callback;
}

void Zesto_Call_WriteByteCallback(unsigned int addr, unsigned char val_to_write)
{
    if (write_byte_call != NULL)
       write_byte_call(addr, val_to_write); 
}
