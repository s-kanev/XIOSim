#ifndef __HANDSHAKE_CONTAINER__
#define __HANDSHAKE_CONTAINER__

#include <assert.h>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

#include "xiosim/host.h"
#include "xiosim/decode.h"

struct handshake_flags_t {
    bool valid : 1; /* Did we finish dumping context */

    bool isFirstInsn : 1;         /* Notify sim for first instruction in a slice */
    bool giveCoreUp : 1;          /* Notify the scheduler to release thread */
    bool giveUpReschedule : 1;    /* When ^ is true, should thread get re-scheduled */
    bool killThread : 1;          /* Thread is exiting, deschedule it and clean up once consumed */
    bool brtaken : 1;             /* Taken or Not-Taken for branch instructions */
    bool flush_pipe : 1;          /* Flush core pipelie */
    bool real : 1;                /* Is this a real instruction */
    bool in_critical_section : 1; /* Thread executing a sequential cut? */
    bool speculative : 1;         /* Is instruction on a wrong path */
};

class handshake_container_t {
  public:
    handshake_container_t() { Invalidate(); }

    void Invalidate() {
        memset(&flags, 0, sizeof(flags));
        flags.real = true;
        mem_buffer.clear();
        pc = 0;
        npc = 0;
        tpc = 0;
        rSP = 0;
        memset(ins, 0, sizeof(ins));
        asid = 0;
    }

    handshake_container_t(const handshake_container_t& rhs)
        : pc(rhs.pc)
        , npc(rhs.npc)
        , tpc(rhs.tpc)
        , rSP(rhs.rSP)
        , asid(rhs.asid)
        , flags(rhs.flags)
        , mem_buffer(rhs.mem_buffer) {
        memcpy(ins, rhs.ins, sizeof(ins));
    }

    size_t Serialize(void* const buffer, size_t buffer_size) {
        /* First, reserve some space for the size of the whole structure. */
        char* buffPosition = (char* const)buffer;
        buffPosition += sizeof(size_t);

        /* Flags */
        const size_t flagBytes = sizeof(handshake_flags_t);
        buffPosition = copyToBuff(buffPosition, &(flags), flagBytes);

        /* Memory vector size, followed by elements */
        int vectorSize = mem_buffer.size();
        const size_t vectorEntryBytes = sizeof(md_addr_t) + sizeof(uint8_t);
        size_t vectorBytes = vectorSize * vectorEntryBytes;
        buffPosition = copyToBuff(buffPosition, &(vectorBytes), sizeof(size_t));

        /* If available, memory accesses */
        for (auto access : mem_buffer) {
            buffPosition = copyToBuff(buffPosition, &(access.first), sizeof(md_addr_t));
            buffPosition = copyToBuff(buffPosition, &(access.second), sizeof(uint8_t));
        }

        /* Registers */
        buffPosition = copyToBuff(buffPosition, &(pc), sizeof(md_addr_t));
        buffPosition = copyToBuff(buffPosition, &(npc), sizeof(md_addr_t));
        buffPosition = copyToBuff(buffPosition, &(tpc), sizeof(md_addr_t));
        buffPosition = copyToBuff(buffPosition, &(rSP), sizeof(md_addr_t));
        /* Instruction bytes */
        buffPosition = copyToBuff(buffPosition, ins, sizeof(ins));
        buffPosition = copyToBuff(buffPosition, &asid, sizeof(asid));

        /* Finally, write the total size */
        size_t totalBytes = buffPosition - (char* const)buffer;
        memcpy(buffer, &(totalBytes), sizeof(size_t));
#ifdef SERIALIZATION_DEBUG
        std::cerr << "[WRITE]TotalBytes: " << totalBytes - sizeof(size_t) << std::endl;
#endif
        return totalBytes;
    }

    void Deserialize(void const* const buffer, size_t buffer_size) {
        const size_t flagBytes = sizeof(handshake_flags_t);
        const size_t vectorEntryBytes = sizeof(md_addr_t) + sizeof(uint8_t);
#ifdef SERIALIZATION_DEBUG
        std::cerr << "[READ]TotalBytes: " << buffer_size << std::endl;
#endif

        char const* buffPosition = (char const*)buffer;

        /* Flags */
        buffPosition = copyFromBuff(&(this->flags), buffPosition, flagBytes);

        /* Memory vector size, followed by elements */
        size_t vectorBytes;
        buffPosition = copyFromBuff(&(vectorBytes), buffPosition, sizeof(size_t));
        assert(vectorBytes % vectorEntryBytes == 0);
        const size_t vectorNum = vectorBytes / vectorEntryBytes;

        this->mem_buffer.clear();
        for (size_t i = 0; i < vectorNum; i++) {
            md_addr_t addr;
            uint8_t size;

            buffPosition = copyFromBuff(&addr, buffPosition, sizeof(md_addr_t));
            buffPosition = copyFromBuff(&size, buffPosition, sizeof(uint8_t));
            this->mem_buffer.push_back(std::make_pair(addr, size));
        }

        /* Regs */
        buffPosition = copyFromBuff(&pc, buffPosition, sizeof(md_addr_t));
        buffPosition = copyFromBuff(&npc, buffPosition, sizeof(md_addr_t));
        buffPosition = copyFromBuff(&tpc, buffPosition, sizeof(md_addr_t));
        buffPosition = copyFromBuff(&rSP, buffPosition, sizeof(md_addr_t));
        /* Instruction bytes */
        buffPosition = copyFromBuff(ins, buffPosition, sizeof(ins));
        buffPosition = copyFromBuff(&asid, buffPosition, sizeof(asid));
    }

    /* Current instruction address */
    md_addr_t pc;
    /* Fallthrough instruction address */
    md_addr_t npc;
    /* Next address Pin will execute */
    md_addr_t tpc;
    /* Stack pointer before executing the instruction */
    md_addr_t rSP;
    /* Instruction bytes */
    uint8_t ins[xiosim::x86::MAX_ILEN];

    /* Address space ID */
    uint8_t asid;

    struct handshake_flags_t flags;

    /* Addresses and sizes of instruction memory accesses. */
    std::vector<std::pair<md_addr_t, uint8_t> > mem_buffer;

    bool operator==(const handshake_container_t& rhs) {
        return memcmp(&flags, &rhs.flags, sizeof(flags)) == 0 && mem_buffer == rhs.mem_buffer &&
               pc == rhs.pc && npc == rhs.npc && tpc == rhs.tpc && rSP == rhs.rSP &&
               memcmp(ins, rhs.ins, sizeof(ins)) == 0 && asid == rhs.asid;
    }

    friend std::ostream& operator<<(std::ostream& out, class handshake_container_t& hand) {
        out << "hand:"
            << " ";
        out << hand.flags.valid;
        out << hand.flags.isFirstInsn;
        out << hand.flags.killThread;
        out << " ";
        out << "mem: " << hand.mem_buffer.size() << " ";
        out << std::hex;
        for (auto access : hand.mem_buffer)
            out << access.first << ": " << (uint32_t)access.second << " ";
        out << " ";
        out << "pc: " << hand.pc << " ";
        out << "npc: " << hand.npc << " ";
        out << "tpc: " << hand.tpc << " ";
        out << std::dec << "brtaken: " << hand.flags.brtaken << " ";
        out << std::hex << "ins: ";
        for (size_t i = 0; i < xiosim::x86::MAX_ILEN; i++)
            out << (uint32_t)hand.ins[i] << " ";
        out << "flags: ";
        out << std::dec << hand.flags.real;
        out << hand.flags.speculative;
        out << hand.flags.in_critical_section;
        out.flush();
        return out;
    }

  private:
    char* copyToBuff(char* buff, void const* addr, const size_t size) const {
        memcpy(buff, addr, size);
        return buff + size;
    }

    char const* copyFromBuff(void* addr, char const* buff, const size_t size) const {
        memcpy(addr, buff, size);
        return buff + size;
    }
};

#endif /*__HANDSHAKE_CONTAINER__ */
