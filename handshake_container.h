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
  bool giveCoreUp;          /* Notify the scheduler to release thread */
  bool giveUpReschedule;    /* When ^ is true, should thread get re-scheduled */
  bool killThread;          /* Thread is exiting, deschedule it and clean up once consumed */
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
        flags.giveCoreUp = false;
        flags.killThread = false;

        mem_buffer.clear();
    }

    void CopyTo(handshake_container_t* dest) const {
        dest->flags = this->flags;
        memcpy(&dest->handshake, &this->handshake, sizeof(P2Z_HANDSHAKE));
        dest->mem_buffer = this->mem_buffer;
    }

    size_t Serialize(void * const buffer, size_t buffer_size) const {
        int mapSize = this->mem_buffer.size();
        const size_t handshakeBytes = sizeof(P2Z_HANDSHAKE);
        const size_t flagBytes = sizeof(handshake_flags_t);
        const size_t mapEntryBytes = sizeof(uint32_t) + sizeof(uint8_t);
        size_t mapBytes = mapSize * mapEntryBytes;
        size_t totalBytes = sizeof(int) + handshakeBytes + flagBytes + mapBytes;

        assert(totalBytes <= buffer_size);

        /* First write the size of the whole structure. */
        char * buffPosition = (char * const)buffer;
        memcpy(buffPosition, &(totalBytes), sizeof(int));
        buffPosition = buffPosition + sizeof(int);

        /* Then, write compressed handshake */
        memcpy(buffPosition, &(this->handshake), handshakeBytes);
        buffPosition = buffPosition + handshakeBytes;

        /* Flags */
        memcpy(buffPosition, &(this->flags), flagBytes);
        buffPosition = buffPosition + flagBytes;

        /* If available, memory accesses */
        for(auto it = this->mem_buffer.begin(); it != this->mem_buffer.end(); it++) {
            memcpy(buffPosition, &(it->first), sizeof(uint32_t));
            buffPosition = buffPosition + sizeof(uint32_t);

            memcpy(buffPosition, &(it->second), sizeof(uint8_t));
            buffPosition = buffPosition + sizeof(uint8_t);
        }
        assert((char*)buffer + totalBytes == buffPosition);
        return totalBytes;
    }

    void Deserialize(void const * const buffer, size_t buffer_size) {
        const size_t handshakeBytes = sizeof(P2Z_HANDSHAKE);
        const size_t flagBytes = sizeof(handshake_flags_t);
        const size_t mapEntryBytes = sizeof(uint32_t) + sizeof(uint8_t);

        const size_t mapBytes = buffer_size - handshakeBytes - flagBytes;
        assert(mapBytes % mapEntryBytes == 0);
        const size_t mapNum = mapBytes / mapEntryBytes;

        char const * buffPosition = (char const *) buffer;
        memcpy(&(this->handshake), buffPosition, handshakeBytes);
        buffPosition = buffPosition + handshakeBytes;

        memcpy(&(this->flags), buffPosition, flagBytes);
        buffPosition = buffPosition + flagBytes;

        this->mem_buffer.clear();
        for(unsigned int i = 0; i < mapNum; i++) {
            uint32_t first;
            uint8_t second;

            first = *((uint32_t*)buffPosition);
            buffPosition = buffPosition + sizeof(uint32_t);

            second = *((uint8_t*)buffPosition);
            buffPosition = buffPosition + sizeof(uint8_t);

            (this->mem_buffer)[first] = second;
        }
        assert(((char*)buffer) + buffer_size == buffPosition);
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
        out.flush();
        return out;
    }
};

#endif /*__HANDSHAKE_CONTAINER__ */
