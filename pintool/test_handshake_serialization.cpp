#define CATCH_CONFIG_MAIN

#include "catch.hpp"
//#define SERIAlIZATION_DEBUG
#include "handshake_container.h"

struct test_context {
    handshake_container_t producer_handshake;
    handshake_container_t consumer_handshake;
    static const size_t buffer_size = 4096;
    char buffer[buffer_size];

    void perform_serialization() {
        memset(buffer, 0, buffer_size);

        producer_handshake.Serialize(buffer, buffer_size);

        size_t bytes_written = *(size_t*)buffer;
        bytes_written -= sizeof(size_t);

        consumer_handshake.Deserialize(buffer + 4, bytes_written);

        REQUIRE(producer_handshake == consumer_handshake);
    }
};

TEST_CASE("Serialize-deserialize test", "handshakes") {
    test_context ctxt;

    SECTION("Empty") { ctxt.perform_serialization(); }

    SECTION("PCs") {
        auto& start = ctxt.producer_handshake;
        start.pc = 0xdeadbeef;
        start.npc = 0xfeedface;
        start.tpc = 0xdecafbad;
        start.rSP = 0x12345678;

        ctxt.perform_serialization();
    }

    SECTION("Memory") {
        auto& start = ctxt.producer_handshake;
        start.mem_buffer.push_back(std::make_pair(0xdeadbeef, 4));
        start.mem_buffer.push_back(std::make_pair(0xfeedface, 4));

        ctxt.perform_serialization();
    }

    SECTION("Flags") {
        auto& start = ctxt.producer_handshake;
        start.flags.speculative = true;
        start.flags.valid = true;
        start.flags.real = true;

        ctxt.perform_serialization();
    }

    SECTION("Instruction") {
        auto& start = ctxt.producer_handshake;
        start.flags.valid = true;
        start.flags.real = true;

        start.pc = 0xdeadbeef;
        start.npc = 0xfeedface;
        start.tpc = 0xdecafbad;
        start.rSP = 0x12345678;

        start.mem_buffer.push_back(std::make_pair(0xdeadbeef, 4));

        start.ins[0] = 0x90;

        ctxt.perform_serialization();
    }
}
