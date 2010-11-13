#ifndef __PIN_ZESTO_INTERFACE__
#define __PIN_ZESTO_INTERFACE__

#include "pin.h"
#include "machine.h"

int Zesto_SlaveInit(int argc, char **argv);
void Zesto_Resume(struct P2Z_HANDSHAKE * handshake);
void Zesto_Destroy();
int Zesto_Notify_Mmap(unsigned int addr, unsigned int length, bool mod_brk);
int Zesto_Notify_Munmap(unsigned int addr, unsigned int length, bool mod_brk);
void Zesto_SetBOS(unsigned int stack_base);
void Zesto_UpdateBrk(unsigned int brk_end);


#endif /*__PIN_ZESTO_INTERFACE__*/

