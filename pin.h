/* 
 * Handshake between Pin and Zesto.
 * Copyright, Svilen Kanev, 2011
*/

#ifndef __PIN_ZESTO_H__
#define __PIN_ZESTO_H__

#include "host.h"
#include "decode.h"

typedef struct P2Z_HANDSHAKE {
    unsigned int pc;                    /* Current instruction address */
    unsigned int npc;                   /* Fallthrough instruction address */
    unsigned int tpc;                   /* Next address Pin will execute */
    unsigned char ins[xiosim::x86::MAX_ILEN];   /* Instruction bytes */
    char asid;                          /* Address space ID */
    /* XXX: This MUST be the last member! (see handshake_container.h) */
    struct regs_t ctxt;                 /* Register context */
} P2Z_HANDSHAKE;

class handshake_container_t;

typedef void (*ZESTO_WRITE_BYTE_CALLBACK) (unsigned int, unsigned char);

#endif /*__PIN_ZESTO_H__*/
