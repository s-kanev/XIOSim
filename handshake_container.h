#ifndef __HANDSHAKE_CONTAINER__
#define __HANDSHAKE_CONTAINER__

#include <iostream>
#include <map>

#include "machine.h"
#include "pin.h"

struct handshake_flags_t
{
  bool valid;               /* Did we finish dumping context */

  bool isFirstInsn;         /* Notify sim for first instruction in a slice */
  bool isLastInsn;          /* Same ^ for last */

  bool giveCoreUp;          /* Notify the scheduler to release thread */
  bool giveUpReschedule;    /* When ^ is true, should thread get re-scheduled */
  bool killThread;          /* Thread is exiting, deschedule it and clean up once consumed */
  md_addr_t BOS;
};

class handshake_container_t
{
  public:
    handshake_container_t() {
        Clear();
    }

    void Clear() {
        memset(&handshake, 0, sizeof(P2Z_HANDSHAKE));
        handshake.real = true;
        flags.valid = false;
        flags.isFirstInsn = false;
        flags.isLastInsn = false;
        flags.giveCoreUp = false;
        flags.killThread = false;

        mem_buffer.clear();
    }

    void CopyTo(handshake_container_t* dest) const {
        dest->flags = this->flags;
        memcpy(&dest->handshake, &this->handshake, sizeof(P2Z_HANDSHAKE));
        dest->mem_buffer = this->mem_buffer;
    }

    // Handshake information that gets passed on to Zesto
    struct P2Z_HANDSHAKE handshake;

    struct handshake_flags_t flags;

    // Memory reads and writes to be passed on to Zesto
    std::map<uint32_t, uint8_t> mem_buffer;

    friend std::ostream& operator<< (std::ostream &out, class handshake_container_t &hand)
    {
        out << "hand:" << " ";
        out << hand.flags.valid;
        out << hand.flags.isFirstInsn;
        out << hand.flags.isLastInsn;
        out << hand.flags.killThread;
        out << " ";
        out << "mema: " << std::hex << hand.handshake.mem_addr << " val: " << hand.handshake.mem_val << std::dec << " ";
        out << hand.mem_buffer.size();
        for (auto it=hand.mem_buffer.begin(); it != hand.mem_buffer.end(); it++)
            out << std::hex << it->first << ": " << (uint32_t)it->second << " ";
        out << " ";
        out << "pc: " << hand.handshake.pc << " ";
        out << "npc: " << hand.handshake.npc << " ";
        out << "tpc: " << hand.handshake.tpc << " ";
        out << std::dec << "brtaken: " << hand.handshake.brtaken << " ";
        out << std::hex << "ins: ";
        for (int i=0; i < MD_MAX_ILEN; i++)
            out << (uint32_t)hand.handshake.ins[i] << " ";
        out << "flags: " << hand.handshake.sleep_thread << hand.handshake.resume_thread;
        out << std::dec << hand.handshake.real;
        out << hand.handshake.in_critical_section;
        out << " slicenum: " << hand.handshake.slice_num << " ";
        out << "feederslicelen: " << hand.handshake.feeder_slice_length << " ";
        out << "feedersliceweight: " << hand.handshake.slice_weight_times_1000 << " ";
        out.flush();
        return out;
    }
};

#endif /*__HANDSHAKE_CONTAINER__ */
