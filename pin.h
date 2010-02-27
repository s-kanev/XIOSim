#ifndef __PIN_ZESTO_H__
#define __PIN_ZESTO_H__

#include "host.h"

typedef struct P2Z_HANDSHAKE {
    unsigned int pc;                    /* Current instruction address */
    unsigned int npc;                   /* Fallthrough instruction address */
    unsigned int tpc;                   /* Next address Pin will execute */
    int brtaken;                        /* Taken or Not-Taken for branch instructions */
    const struct regs_t *ctxt;          /* Register context */
    int orig;                           /* Original program instruction */
    int icount;                         /* Dynamic program instruction sequence id */
    unsigned char *ins;                 /* Instruction bytes */
} P2Z_HANDSHAKE;

void Zesto_Init();
void Zesto_Destroy();
void Zesto_Resume(struct P2Z_HANDSHAKE * handshake);


#endif /*__PIN_ZESTO_H__*/
