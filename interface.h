/* 
 * Interface to instruction feeder.
 * Copyright, Svilen Kanev, 2011
*/

#ifndef __PIN_ZESTO_INTERFACE__
#define __PIN_ZESTO_INTERFACE__

#include "machine.h"
#include "pin.h"

/* Calls from feeder to Zesto */
int Zesto_SlaveInit(int argc, char **argv);
void Zesto_Resume(struct P2Z_HANDSHAKE * handshake, std::map<unsigned int, unsigned char> * mem_buffer, bool start_slice, bool end_slice);
void Zesto_Destroy();
int Zesto_Notify_Mmap(int coreID, unsigned int addr, unsigned int length, bool mod_brk);
int Zesto_Notify_Munmap(int coreID, unsigned int addr, unsigned int length, bool mod_brk);
void Zesto_SetBOS(int coreID, unsigned int stack_base);
void Zesto_UpdateBrk(int coreID, unsigned int brk_end, bool do_mmap);

void Zesto_Add_WriteByteCallback(ZESTO_WRITE_BYTE_CALLBACK callback);
void Zesto_WarmLLC(unsigned int addr, bool is_write);

void Zesto_Slice_End(int coreID, unsigned int slice_num, unsigned long long feeder_slice_length, unsigned long long slice_weight_times_1000);

void Zesto_Start_Parallel_Loop();

void deactivate_core(int coreID);
extern bool sim_release_handshake;
extern int num_threads;

#endif /*__PIN_ZESTO_INTERFACE__*/

