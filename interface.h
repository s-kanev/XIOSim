#ifndef __PIN_ZESTO_INTERFACE__
#define __PIN_ZESTO_INTERFACE__

#include "pin.h"
#include "machine.h"

void Zesto_Init(INT32 argc, CHAR **argv);
void Zesto_Destroy();
void Zesto_Resume(struct P2Z_HANDSHAKE * handshake);

#endif /*__PIN_ZESTO_INTERFACE__*/

