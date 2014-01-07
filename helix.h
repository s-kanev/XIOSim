#ifndef __HELIX_H__
#define __HELIX_H__

const unsigned int HELIX_WAIT_MASK = 0x10000;
inline bool is_addr_helix_signal(unsigned int addr) { return !(addr & HELIX_WAIT_MASK); }
inline bool is_uop_helix_signal(const struct uop_t * uop) { 
    return (uop->decode.is_sta || uop->decode.is_std) &&
            uop->oracle.is_sync_op &&
            is_addr_helix_signal(uop->oracle.virt_addr);
}

#endif /* __HELIX_H__ */
