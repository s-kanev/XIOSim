#ifndef __HANDSHAKE_CONTAINER__
#define __HANDSHAKE_CONTAINER__

#include <iostream>
#include <map>

#include "../machine.h"
#include "../pin.h"

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
};

class handshake_container_t {
  public:
    handshake_container_t() { Clear(); }

    void Clear() {
        memset(&handshake, 0, sizeof(P2Z_HANDSHAKE));
        memset(&flags, 0, sizeof(flags));
        flags.real = true;
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

    static const dword_t WORD_MASK = 0x80000000;
    static const dword_t BYTE_MASK = 0x40000000;

    size_t Serialize(void* const buffer, size_t buffer_size, regs_t const* const shadow_regs) {
        int mapSize = this->mem_buffer.size();
#ifndef NDEBUG
        const size_t handshakeBytes = sizeof(P2Z_HANDSHAKE);
#endif
        const size_t flagBytes = sizeof(handshake_flags_t);
        const size_t mapEntryBytes = sizeof(uint32_t) + sizeof(uint8_t);
        size_t mapBytes = mapSize * mapEntryBytes;

#ifndef NDEBUG
        size_t maxBytes = sizeof(int) + handshakeBytes + flagBytes + mapBytes;
#endif
        assert(maxBytes <= buffer_size);

        /* First, reserve some space for the size of the whole structure. */
        char* buffPosition = (char* const)buffer;
        buffPosition += sizeof(size_t);

        /* Flags */
        memcpy(buffPosition, &(this->flags), flagBytes);
        buffPosition += flagBytes;

        /* Map size, followed by map elements */
        memcpy(buffPosition, &(mapBytes), sizeof(size_t));
        buffPosition += sizeof(size_t);

        /* If available, memory accesses */
        for (auto it = this->mem_buffer.begin(); it != this->mem_buffer.end(); it++) {
            memcpy(buffPosition, &(it->first), sizeof(uint32_t));
            buffPosition += sizeof(uint32_t);

            memcpy(buffPosition, &(it->second), sizeof(uint8_t));
            buffPosition += sizeof(uint8_t);
        }

        /* Compressed handshake */
        buffPosition = WriteCompressedHandshake(buffPosition, shadow_regs);

        /* Finally, write the total size */
        size_t totalBytes = buffPosition - (char* const)buffer;
        memcpy(buffer, &(totalBytes), sizeof(size_t));
#ifdef COMPRESSION_DEBUG
        std::cerr << "[WRITE]TotalBytes: " << totalBytes - sizeof(size_t) << std::endl;
#endif

        return totalBytes;
    }

    void Deserialize(void const* const buffer, size_t buffer_size) {
        const size_t flagBytes = sizeof(handshake_flags_t);
        const size_t mapEntryBytes = sizeof(uint32_t) + sizeof(uint8_t);
#ifdef COMPRESSION_DEBUG
        std::cerr << "[READ]TotalBytes: " << buffer_size << std::endl;
#endif

        char const* buffPosition = (char const*)buffer;

        /* Flags */
        memcpy(&(this->flags), buffPosition, flagBytes);
        buffPosition += flagBytes;

        /* Map size, followed by map elements */
        size_t mapBytes;
        memcpy(&(mapBytes), buffPosition, sizeof(size_t));
        buffPosition += sizeof(size_t);
        assert(mapBytes % mapEntryBytes == 0);
        const size_t mapNum = mapBytes / mapEntryBytes;

        this->mem_buffer.clear();
        for (unsigned int i = 0; i < mapNum; i++) {
            uint32_t first;
            uint8_t second;

            first = *((uint32_t*)buffPosition);
            buffPosition += sizeof(uint32_t);

            second = *((uint8_t*)buffPosition);
            buffPosition += sizeof(uint8_t);

            (this->mem_buffer)[first] = second;
        }

        const size_t compressedBytes = buffer_size - flagBytes - sizeof(size_t) - mapBytes;
        assert(compressedBytes > 0);

        /* Compressed handshake */
        buffPosition = DecompressHandshake(buffPosition, compressedBytes);
        assert(buffPosition == (char*)buffer + buffer_size);
    }

    char*
    WriteCompressedToBuffer(char* buffer, size_t offset, const void* const val, size_t val_size) {
        assert(offset < sizeof(P2Z_HANDSHAKE));
        assert(val_size <= sizeof(dword_t));
        /* Stupid trick. Most delta entries are dword-aligned. For the odd
         * cases that have a smaller granularity, just set the highest-order
         * bits in the offset. */
        if (val_size == sizeof(word_t))
            offset |= WORD_MASK;
        else if (val_size == sizeof(byte_t))
            offset |= BYTE_MASK;

        /* Write the offset -- relative to the P2Z_HANDSHAKE struct */
        *(size_t*)buffer = offset;
        buffer += sizeof(size_t);
        /* Write the actual new data */
        memcpy(buffer, val, val_size);
        /* Advance buffer with 4 bytes regardless */
        buffer += sizeof(dword_t);
        return buffer;
    }

    char* WriteCompressedHandshake(char* buffer, regs_t const* const shadow_regs) {
        const size_t regsOffset = offsetof(P2Z_HANDSHAKE, ctxt);

        /* Write everything before register state */
        memcpy(buffer, &(this->handshake), regsOffset);
        buffer += regsOffset;

        /* Write things from reg state that we didn't compress */
        memcpy(buffer, &(this->handshake.ctxt.regs_PC), sizeof(md_addr_t));
        buffer += sizeof(md_addr_t);
        memcpy(buffer, &(this->handshake.ctxt.regs_NPC), sizeof(md_addr_t));
        buffer += sizeof(md_addr_t);

        /* Write IREGS */
        const size_t iregOffset = offsetof(regs_t, regs_R);
        for (size_t i=0; i < xiosim::x86::NUM_IREGS; i++) {
            if (this->handshake.ctxt.regs_R.dw[i] != shadow_regs->regs_R.dw[i]) {
                /* Offset -- relative to the P2Z_HANDSHAKE struct */
                size_t offset = regsOffset + iregOffset + i * sizeof(dword_t);
                buffer = WriteCompressedToBuffer(
                    buffer, offset, &this->handshake.ctxt.regs_R.dw[i], sizeof(dword_t));
            }
        }

        /* Write FREGS */
        const size_t fregOffset = offsetof(regs_t, regs_F);
        for (size_t i=0; i < xiosim::x86::NUM_FREGS; i++) {
            if (memcmp(&this->handshake.ctxt.regs_F.e[i],
                       &shadow_regs->regs_F.e[i],
                       sizeof(shadow_regs->regs_F.e[0]))) {
                for (size_t float_ind = 0; float_ind < xiosim::x86::FREG_HOST_SIZE / sizeof(sfloat_t);
                     float_ind++) {
                    size_t offset = regsOffset + fregOffset + i * xiosim::x86::FREG_HOST_SIZE +
                                    float_ind * sizeof(sfloat_t);
                    buffer = WriteCompressedToBuffer(buffer,
                                                     offset,
                                                     &this->handshake.ctxt.regs_F.f[i][float_ind],
                                                     sizeof(sfloat_t));
                }
            }
        }

        /* Write control regs */
        const size_t ctrlOffset = offsetof(regs_t, regs_C);
        if (this->handshake.ctxt.regs_C.aflags != shadow_regs->regs_C.aflags) {
            size_t offset = regsOffset + ctrlOffset + offsetof(md_ctrl_t, aflags);
            buffer = WriteCompressedToBuffer(
                buffer, offset, &this->handshake.ctxt.regs_C.aflags, sizeof(dword_t));
        }
        if (this->handshake.ctxt.regs_C.cwd != shadow_regs->regs_C.cwd) {
            size_t offset = regsOffset + ctrlOffset + offsetof(md_ctrl_t, cwd);
            buffer = WriteCompressedToBuffer(
                buffer, offset, &this->handshake.ctxt.regs_C.cwd, sizeof(word_t));
        }
        if (this->handshake.ctxt.regs_C.fsw != shadow_regs->regs_C.fsw) {
            size_t offset = regsOffset + ctrlOffset + offsetof(md_ctrl_t, fsw);
            buffer = WriteCompressedToBuffer(
                buffer, offset, &this->handshake.ctxt.regs_C.fsw, sizeof(word_t));
        }
        if (this->handshake.ctxt.regs_C.ftw != shadow_regs->regs_C.ftw) {
            size_t offset = regsOffset + ctrlOffset + offsetof(md_ctrl_t, ftw);
            buffer = WriteCompressedToBuffer(
                buffer, offset, &this->handshake.ctxt.regs_C.ftw, sizeof(byte_t));
        }

        /* Write segment regs */
        const size_t segOffset = offsetof(regs_t, regs_S);
        for (size_t i=0; i < xiosim::x86::NUM_SREGS; i++) {
            if (this->handshake.ctxt.regs_S.w[i] != shadow_regs->regs_S.w[i]) {
                size_t offset = regsOffset + segOffset + i * sizeof(word_t);
                buffer = WriteCompressedToBuffer(
                    buffer, offset, &this->handshake.ctxt.regs_S.w[i], sizeof(word_t));
            }
        }

        /* Write segment base regs */
        const size_t segBaseOffset = offsetof(regs_t, regs_SD);
        for (size_t i=0; i < xiosim::x86::NUM_SREGS; i++) {
            if (this->handshake.ctxt.regs_SD.dw[i] != shadow_regs->regs_SD.dw[i]) {
                size_t offset = regsOffset + segBaseOffset + i * sizeof(dword_t);
                buffer = WriteCompressedToBuffer(
                    buffer, offset, &this->handshake.ctxt.regs_SD.dw[i], sizeof(dword_t));
            }
        }

        /* Write XMMS */
        const size_t xregOffset = offsetof(regs_t, regs_XMM);
        for (size_t i=0; i < xiosim::x86::NUM_XMMREGS; i++) {
            if (memcmp(&this->handshake.ctxt.regs_XMM.qw[i],
                       &shadow_regs->regs_XMM.qw[i],
                       sizeof(shadow_regs->regs_XMM.qw[0]))) {
                for (size_t float_ind = 0; float_ind < xiosim::x86::XMMREG_SIZE / sizeof(sfloat_t);
                     float_ind++) {
                    /* Write the offset -- relative to the P2Z_HANDSHAKE struct */ 
                    size_t offset =
                        regsOffset + xregOffset + i * xiosim::x86::XMMREG_SIZE +
                        float_ind * sizeof(sfloat_t);
                    buffer = WriteCompressedToBuffer(buffer,
                                                    offset,
                                                    &this->handshake.ctxt.regs_XMM.f[i][float_ind],
                                                    sizeof(sfloat_t));
                }
            }
        }

        return buffer;
    }

    char const* DecompressHandshake(char const* buffer, size_t compressed_bytes) {
        const size_t regsOffset = offsetof(P2Z_HANDSHAKE, ctxt);

        /* Read everything before register state */
        memcpy(&(this->handshake), buffer, regsOffset);
        buffer += regsOffset;

        /* Read things from reg state that we didn't compress */
        memcpy(&(this->handshake.ctxt.regs_PC), buffer, sizeof(md_addr_t));
        buffer += sizeof(md_addr_t);
        memcpy(&(this->handshake.ctxt.regs_NPC), buffer, sizeof(md_addr_t));
        buffer += sizeof(md_addr_t);

        size_t deltaBytes = compressed_bytes - regsOffset - sizeof(md_addr_t) - sizeof(md_addr_t);
        assert(deltaBytes >= 0);
        const size_t deltaEntryBytes = sizeof(size_t) + sizeof(dword_t);
        assert(deltaBytes % deltaEntryBytes == 0);
        size_t deltaEntries = deltaBytes / deltaEntryBytes;

        /* Now read and apply the delta entries directly to handshake */
        while (deltaEntries) {
            /* Read offset from P2Z_handshake. */
            size_t offset = *(size_t*)buffer;
            buffer += sizeof(size_t);

            /* And the value that is stored there (offset bits can encode
             * smaller-granularity accesses). */
            size_t size = sizeof(dword_t);
            if (offset & BYTE_MASK) {
                offset &= ~BYTE_MASK;
                size = sizeof(byte_t);
            } else if (offset & WORD_MASK) {
                offset &= ~WORD_MASK;
                size = sizeof(word_t);
            }

            assert(offset < sizeof(this->handshake));
            memcpy(((char*)&this->handshake) + offset, buffer, size);

            /* Buffer entry took 4 bytes regardless of size */
            buffer += sizeof(dword_t);
            deltaEntries--;
        }

        return buffer;
    }

    // Handshake information that gets passed on to Zesto
    struct P2Z_HANDSHAKE handshake;

    struct handshake_flags_t flags;

    // Memory reads and writes to be passed on to Zesto
    std::map<uint32_t, uint8_t> mem_buffer;

    bool operator==(handshake_container_t& rhs) {
        return memcmp(&flags, &rhs.flags, sizeof(flags)) == 0 &&
               memcmp(&handshake, &rhs.handshake, sizeof(handshake)) == 0 &&
               mem_buffer == rhs.mem_buffer;
    }

    friend std::ostream& operator<<(std::ostream& out, class handshake_container_t& hand) {
        out << "hand:"
            << " ";
        out << hand.flags.valid;
        out << hand.flags.isFirstInsn;
        out << hand.flags.killThread;
        out << " ";
        out << "mem: " << hand.mem_buffer.size() << " ";
        for (auto it = hand.mem_buffer.begin(); it != hand.mem_buffer.end(); it++)
            out << std::hex << it->first << ": " << (uint32_t)it->second << " ";
        out << " ";
        out << "pc: " << hand.handshake.pc << " ";
        out << "npc: " << hand.handshake.npc << " ";
        out << "tpc: " << hand.handshake.tpc << " ";
        out << std::dec << "brtaken: " << hand.flags.brtaken << " ";
        out << std::hex << "ins: ";
        for (size_t i=0; i < xiosim::x86::MAX_ILEN; i++)
            out << (uint32_t)hand.handshake.ins[i] << " ";
        out << "flags: ";
        out << std::dec << hand.flags.real;
        out << hand.flags.in_critical_section;
        out.flush();
        return out;
    }
};

#endif /*__HANDSHAKE_CONTAINER__ */
