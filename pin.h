/* 
 * Handshake between Pin and Zesto.
 * Copyright, Svilen Kanev, 2011
*/

#ifndef __PIN_ZESTO_H__
#define __PIN_ZESTO_H__

#include "host.h"

typedef struct P2Z_HANDSHAKE {
    unsigned int pc;                    /* Current instruction address */
    unsigned int coreID;                /* CoreID that should execute instruction */
    unsigned int npc;                   /* Fallthrough instruction address */
    unsigned int tpc;                   /* Next address Pin will execute */
    int brtaken;                        /* Taken or Not-Taken for branch instructions */
    struct regs_t ctxt;                 /* Register context */
    unsigned char ins[6];               /* Instruction bytes */
    bool sleep_thread;                  /* Deactivate core */
    bool resume_thread;                 /* Re-activate core */
    bool real;                          /* Is this a real instruction */

    unsigned int slice_num;                         /* Execution slice id */
    unsigned long long feeder_slice_length;         /* Slice length as seen by pin */
    unsigned long long slice_weight_times_1000;     /* Slice weight in average */
} P2Z_HANDSHAKE;

typedef void (*ZESTO_WRITE_BYTE_CALLBACK) (unsigned int, unsigned char);

#endif /*__PIN_ZESTO_H__*/
