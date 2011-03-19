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

typedef void (*ZESTO_WRITE_BYTE_CALLBACK) (unsigned int, unsigned char);

#endif /*__PIN_ZESTO_H__*/
