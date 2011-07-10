/* 
 * Interface to instruction feeder.
 * Copyright, Svilen Kanev, 2011
*/

#ifndef __PIN_ZESTO_INTERFACE__
#define __PIN_ZESTO_INTERFACE__

#include "pin.h"
#include "machine.h"

int Zesto_SlaveInit(int argc, char **argv);
void Zesto_Resume(struct P2Z_HANDSHAKE * handshake, bool start_slice, bool end_slice);
void Zesto_Destroy();
int Zesto_Notify_Mmap(unsigned int addr, unsigned int length, bool mod_brk);
int Zesto_Notify_Munmap(unsigned int addr, unsigned int length, bool mod_brk);
void Zesto_SetBOS(unsigned int stack_base);
void Zesto_UpdateBrk(unsigned int brk_end, bool do_mmap);
void Zesto_SliceStart(unsigned int slice_num);
void Zesto_SliceEnd(unsigned int slice_num, unsigned long long slice_weight_times_1000, unsigned long long slice_length);

void Zesto_Add_WriteByteCallback(ZESTO_WRITE_BYTE_CALLBACK callback);

#endif /*__PIN_ZESTO_INTERFACE__*/

