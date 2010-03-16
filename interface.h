#ifndef __PIN_ZESTO_INTERFACE__
#define __PIN_ZESTO_INTERFACE__

#define ZESTO_PIN

#include "pin.h"
#include "machine.h"

int Zesto_SlaveInit(int argc, char **argv);
void Zesto_Destroy();
void Zesto_Resume(struct P2Z_HANDSHAKE * handshake);
int Zesto_Notify_Mmap(unsigned int addr, unsigned int length);
int Zesto_Notify_Munmap(unsigned int addr, unsigned int length);


#endif /*__PIN_ZESTO_INTERFACE__*/

