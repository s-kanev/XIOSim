#ifndef __HELIX_H__
#define __HELIX_H__

#include "zesto-structs.h"

// bits 9..0 for the signal id
const unsigned int HELIX_SIGNAL_ID_MASK = 0x3ff;
// bits 14..10 is the first core id
const unsigned int HELIX_SIGNAL_FIRST_CORE_MASK = 0x7c00;
const unsigned int HELIX_SIGNAL_FIRST_CORE_SHIFT = 10;
// bit 15 for light vs. heavy wait
const unsigned int HELIX_LIGHT_WAIT_MASK = 0x8000;
// bit 16 for wait vs. signal
const unsigned int HELIX_WAIT_MASK = 0x10000;

const unsigned int HELIX_SYNC_SIGNAL_ID = 1020;
const unsigned int HELIX_COLLECT_SIGNAL_ID = 1021;
const unsigned int HELIX_FINISH_SIGNAL_ID = 1022;

const unsigned int HELIX_FLUSH_SIGNAL_ID = 1023;
const unsigned int HELIX_MAX_SIGNAL_ID = 1023;

inline bool is_addr_helix_signal(unsigned int addr)
{
    return !(addr & HELIX_WAIT_MASK);
}

inline bool is_uop_helix_signal(const struct uop_t * uop)
{ 
    return (uop->decode.is_sta || uop->decode.is_std) &&
            uop->oracle.is_sync_op &&
            is_addr_helix_signal(uop->oracle.virt_addr);
}

inline unsigned int get_helix_signal_id(unsigned int addr)
{
    return (addr & HELIX_SIGNAL_ID_MASK);
}

inline unsigned int get_helix_signal_first_core(unsigned int addr)
{
    return (addr & HELIX_SIGNAL_FIRST_CORE_MASK) >> HELIX_SIGNAL_FIRST_CORE_SHIFT;
}

inline bool is_helix_signal_collect(unsigned int addr)
{
    return (get_helix_signal_id(addr) == HELIX_COLLECT_SIGNAL_ID);
}

inline bool is_helix_signal_sync(unsigned int addr)
{
    return (get_helix_signal_id(addr) == HELIX_SYNC_SIGNAL_ID);
}

inline bool is_helix_signal_finish(unsigned int addr)
{
    return (get_helix_signal_id(addr) == HELIX_FINISH_SIGNAL_ID);
}


inline bool is_helix_signal_flush(unsigned int addr)
{
    return (get_helix_signal_id(addr) == HELIX_FLUSH_SIGNAL_ID);
}

inline bool is_helix_wait_light(unsigned int addr)
{
    return ((addr & HELIX_LIGHT_WAIT_MASK) == HELIX_LIGHT_WAIT_MASK);
}

#endif /* __HELIX_H__ */
