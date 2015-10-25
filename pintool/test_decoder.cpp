#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#define DECODE_DEBUG
#include "test_xed_context.h"

TEST_CASE("nop", "Flags tests") {
    xed_context c;
    xed_inst0(&c.x, c.dstate, XED_ICLASS_NOP, 0);
    c.encode();

    c.decode();

    REQUIRE(c.Mop.decode.is_ctrl == false);
    REQUIRE(c.Mop.decode.is_trap == false);
    REQUIRE(c.Mop.decode.opflags.CTRL == false);
}

TEST_CASE("RR logic", "Flags tests") {
    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_ADD, 0,
              xed_reg(XED_REG_EAX),
              xed_reg(XED_REG_EDX));
    c.encode();

    c.decode();

    REQUIRE(c.Mop.decode.is_ctrl == false);
    REQUIRE(c.Mop.decode.is_trap == false);
    REQUIRE(c.Mop.decode.opflags.COND == false);
    REQUIRE(c.Mop.decode.opflags.UNCOND == false);
}

TEST_CASE("load-op-store", "Flags tests") {
    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_ADD, 0,
              xed_mem_b(XED_REG_EDX, 32),
              xed_reg(XED_REG_EAX));
    c.encode();

    c.decode();

    REQUIRE(c.Mop.decode.opflags.MEM == true);
    REQUIRE(c.Mop.decode.opflags.LOAD == true);
    REQUIRE(c.Mop.decode.opflags.STORE == true);
}

TEST_CASE("jmp direct", "Flags tests") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_JMP, 0,
              xed_relbr(0xdeadbeef - 5, 32));
    c.encode();

    c.decode();

    REQUIRE(c.Mop.decode.is_ctrl == true);
    REQUIRE(c.Mop.decode.opflags.UNCOND == true);
    REQUIRE(c.Mop.decode.opflags.INDIR == false);
}

TEST_CASE("jmp indirect", "Flags tests") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_JMP, 0,
              xed_reg(XED_REG_EAX));
    c.encode();

    c.decode();

    REQUIRE(c.Mop.decode.is_ctrl == true);
    REQUIRE(c.Mop.decode.opflags.UNCOND == true);
    REQUIRE(c.Mop.decode.opflags.COND == false);
    REQUIRE(c.Mop.decode.opflags.INDIR == true);
}

TEST_CASE("je", "Flags tests") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_JZ, 0,
              xed_relbr(0xdeadbeef - 6, 32));
    c.encode();

    c.decode();

    REQUIRE(c.Mop.decode.is_ctrl == true);
    REQUIRE(c.Mop.decode.opflags.COND == true);
    REQUIRE(c.Mop.decode.opflags.INDIR == false);
}

TEST_CASE("call direct", "Flags tests") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_CALL_NEAR, 0,
              xed_relbr(0xdeadbeef - 5, 32));
    c.encode();

    c.decode();

    REQUIRE(c.Mop.decode.is_ctrl == true);
    REQUIRE(c.Mop.decode.opflags.UNCOND == true);
    REQUIRE(c.Mop.decode.opflags.INDIR == false);
    REQUIRE(c.Mop.decode.opflags.CALL == true);
    REQUIRE(c.Mop.decode.opflags.COND == false);
}

TEST_CASE("call indirect", "Flags tests") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_CALL_NEAR, 0,
              xed_mem_b(XED_REG_EAX, 32));
    c.encode();

    c.decode();

    REQUIRE(c.Mop.decode.is_ctrl == true);
    REQUIRE(c.Mop.decode.opflags.UNCOND == true);
    REQUIRE(c.Mop.decode.opflags.INDIR == true);
    REQUIRE(c.Mop.decode.opflags.CALL == true);
    REQUIRE(c.Mop.decode.opflags.MEM == true);
    REQUIRE(c.Mop.decode.opflags.LOAD == true);
    REQUIRE(c.Mop.decode.opflags.STORE == true);
}
