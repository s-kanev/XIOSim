/* 
 * Handshake between Pin and Zesto.
 * Copyright, Svilen Kanev, 2011
*/

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

    unsigned int slice_num;
    unsigned long long feeder_slice_length;
    unsigned long long slice_weight_times_1000;
} P2Z_HANDSHAKE;

typedef void (*ZESTO_WRITE_BYTE_CALLBACK) (unsigned int, unsigned char);

#endif /*__PIN_ZESTO_H__*/
