#ifndef __HELIX_H__
#define __HELIX_H__

// bits 9..0 for the signal id
const unsigned int HELIX_SIGNAL_ID_MASK = 0x3ff;
// bits 15..10 is the first core id
const unsigned int HELIX_SIGNAL_FIRST_CORE_MASK = 0xfc00;
const unsigned int HELIX_SIGNAL_FIRST_CORE_SHIFT = 10;
// bit 16 for wait vs. signal
const unsigned int HELIX_WAIT_MASK = 0x10000;

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

inline bool is_helix_signal_flush(unsigned int ssID)
{
    return (ssID == HELIX_FLUSH_SIGNAL_ID);
}

inline unsigned int get_helix_signal_id(unsigned int addr)
{
    return (addr & HELIX_SIGNAL_ID_MASK);
}

inline unsigned int get_helix_signal_first_core(unsigned int addr)
{
    return (addr & HELIX_SIGNAL_FIRST_CORE_MASK) >> HELIX_SIGNAL_FIRST_CORE_SHIFT;
}

#endif /* __HELIX_H__ */
