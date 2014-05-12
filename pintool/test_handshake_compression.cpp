#define CATCH_CONFIG_MAIN

#include "catch.hpp"
//#define COMPRESSION_DEBUG
#include "../handshake_container.h"

struct test_context {
    handshake_container_t producer_handshake;
    handshake_container_t consumer_handshake;
    regs_t producer_shadow;
    regs_t consumer_shadow;
    static const size_t buffer_size = 4096;
    char buffer[buffer_size];

    void perform_serialization() {
        memset(buffer, 0, buffer_size);

        producer_handshake.Serialize(buffer, buffer_size, &producer_shadow);

        size_t bytes_written = *(size_t*)buffer;
        bytes_written -= sizeof(size_t);

        memcpy(&(consumer_handshake.handshake.ctxt), &consumer_shadow, sizeof(regs_t));
        consumer_handshake.Deserialize(buffer + 4, bytes_written);

        REQUIRE(producer_handshake == consumer_handshake);

        memcpy(&producer_shadow, &(producer_handshake.handshake.ctxt), sizeof(regs_t));
        memcpy(&consumer_shadow, &(consumer_handshake.handshake.ctxt), sizeof(regs_t));
    }
};


TEST_CASE("Compression-decompression test", "compression") {
    test_context ctxt;
    memset(&ctxt.producer_shadow, 0, sizeof(regs_t));
    memset(&ctxt.consumer_shadow, 0, sizeof(regs_t));

    SECTION("Empty") {
        ctxt.perform_serialization();
    }

    SECTION("INT state") {
        ctxt.producer_handshake.handshake.ctxt.regs_R.dw[MD_REG_EAX] = 0xdeadbeef;
        ctxt.producer_handshake.handshake.ctxt.regs_R.dw[MD_REG_ECX] = 0xdeadbeef;
        ctxt.producer_handshake.handshake.ctxt.regs_R.dw[MD_NUM_IREGS-1] = 0xdeadbeef;

        ctxt.perform_serialization();
    }

    SECTION("C and S state -- byte and word writes") {
        ctxt.producer_handshake.handshake.ctxt.regs_C.ftw = 0xce;
        ctxt.producer_handshake.handshake.ctxt.regs_S.w[1] = 0xface;

        ctxt.perform_serialization();
    }

    SECTION("More INT state") {
        ctxt.producer_handshake.handshake.ctxt.regs_R.dw[MD_REG_EAX] = 0xdeadbeef;
        ctxt.producer_handshake.handshake.ctxt.regs_R.dw[MD_REG_ECX] = 0xdeadbeef;
        ctxt.producer_handshake.handshake.ctxt.regs_R.dw[MD_NUM_IREGS-1] = 0xdeadbeef;

        ctxt.perform_serialization();

        ctxt.producer_handshake.handshake.ctxt.regs_R.dw[MD_REG_EAX] = 0xfeedface;
        ctxt.producer_handshake.handshake.ctxt.regs_R.dw[MD_REG_ECX] = 0xfeedface;
        ctxt.producer_handshake.handshake.ctxt.regs_R.dw[MD_NUM_IREGS-1] = 0xfeedface;

        ctxt.perform_serialization();
    }

    SECTION("FP state") {
        ctxt.producer_handshake.handshake.ctxt.regs_F.e[0] = M_PI;
        ctxt.producer_handshake.handshake.ctxt.regs_F.e[3] = M_PI;
        ctxt.producer_handshake.handshake.ctxt.regs_F.e[MD_NUM_FREGS-1] = M_PI;

        ctxt.perform_serialization();
    }

    SECTION("XMM state") {
        ctxt.producer_handshake.handshake.ctxt.regs_XMM.qw[0].lo = 0xfeedface;
        ctxt.producer_handshake.handshake.ctxt.regs_XMM.qw[0].hi = 0xfeedface;
        ctxt.producer_handshake.handshake.ctxt.regs_XMM.qw[3].lo = 0xfeedface;
        ctxt.producer_handshake.handshake.ctxt.regs_XMM.qw[3].hi = 0xfeedface;
        ctxt.producer_handshake.handshake.ctxt.regs_XMM.qw[MD_NUM_XMMREGS-1].lo = 0xfeedface;
        ctxt.producer_handshake.handshake.ctxt.regs_XMM.qw[MD_NUM_XMMREGS-1].hi = 0xfeedface;

        ctxt.perform_serialization();
    }

    SECTION("Control state") {
        ctxt.producer_handshake.handshake.ctxt.regs_C.aflags = 0xfeedface;
        ctxt.producer_handshake.handshake.ctxt.regs_C.cwd = 0xfeed;
        ctxt.producer_handshake.handshake.ctxt.regs_C.fsw = 0xface;

        ctxt.perform_serialization();
    }

    SECTION("Segment base state") {
        ctxt.producer_handshake.handshake.ctxt.regs_SD.dw[0] = 0xfeedface;
        ctxt.producer_handshake.handshake.ctxt.regs_SD.dw[MD_NUM_SREGS-1] = 0xfeedface;

        ctxt.perform_serialization();
    }

    SECTION("Segment selector state") {
        ctxt.producer_handshake.handshake.ctxt.regs_S.w[0] = 0xfeed;
        ctxt.producer_handshake.handshake.ctxt.regs_S.w[1] = 0xface;
        ctxt.producer_handshake.handshake.ctxt.regs_S.w[MD_NUM_SREGS-2] = 0xfeed;
        ctxt.producer_handshake.handshake.ctxt.regs_S.w[MD_NUM_SREGS-1] = 0xface;

        ctxt.perform_serialization();
    }

    SECTION("Partially overlapping") {
        ctxt.producer_handshake.handshake.ctxt.regs_S.w[1] = 0xface;

        ctxt.perform_serialization();

        ctxt.producer_handshake.handshake.ctxt.regs_S.w[0] = 0xfeed;

        ctxt.perform_serialization();
    }
}
