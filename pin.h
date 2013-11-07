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
    struct regs_t ctxt;                 /* Register context */
    unsigned char ins[MD_MAX_ILEN];     /* Instruction bytes */
    bool brtaken;                       /* Taken or Not-Taken for branch instructions */
    bool sleep_thread;                  /* Deactivate core */
    bool resume_thread;                 /* Re-activate core */
    bool flush_pipe;                    /* Flush core pipelie */
    bool real;                          /* Is this a real instruction */
    bool in_critical_section;           /* Thread executing a sequential cut? */

                                        /* Shortcut for <= 4 byte mem reads */
    unsigned char mem_size;             /* size of access */
    unsigned long mem_addr;             /* adress of the access */
    unsigned long mem_val;              /* value to be read */

    unsigned int slice_num;                         /* Execution slice id */
    unsigned long long feeder_slice_length;         /* Slice length as seen by pin */
    unsigned long long slice_weight_times_1000;     /* Slice weight in average */
} P2Z_HANDSHAKE;

class handshake_container_t;

typedef void (*ZESTO_WRITE_BYTE_CALLBACK) (unsigned int, unsigned char);

#endif /*__PIN_ZESTO_H__*/
