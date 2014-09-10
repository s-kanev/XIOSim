#define CATCH_CONFIG_MAIN

#include "catch.hpp"

extern "C" {
#include "xed-interface.h"
}

#include "../decode.h"
#include "../uop_cracker.h"

#define DECODE_DEBUG


class xed_context {
  public:
    xed_encoder_instruction_t x; 
    xed_encoder_request_t enc_req;
    xed_state_t dstate;

    struct Mop_t Mop;

    xed_context() {
        xed_tables_init();
        xed_state_zero(&dstate);
        dstate.mmode=XED_MACHINE_MODE_LEGACY_32;
        dstate.stack_addr_width=XED_ADDRESS_WIDTH_32b;

        init_decoder();
    }

    void encode(void) {
        xed_encoder_request_zero_set_mode(&enc_req, &dstate);

        bool convert_ok = xed_convert_to_encoder_request(&enc_req, &x);
        if (!convert_ok) {
            fatal("conversion to encode request failed\n");
        }
        unsigned int inst_len;
        auto err = xed_encode(&enc_req, Mop.fetch.inst.code, MAX_ILEN, &inst_len);
        if (err != XED_ERROR_NONE) {
            fatal("xed_encode failed %s\n", xed_error_enum_t2str(err));
        }
    }

    void decode_and_crack(void) {
        decode(&Mop);
        decode_flags(&Mop);

#ifdef DECODE_DEBUG
        std::cerr << print_Mop(&Mop) << std::endl;
#endif

        crack(&Mop);
    }
};


TEST_CASE("nop", "[uop]") {
    using namespace xiosim::x86;

    xed_context c;
    xed_inst0(&c.x, c.dstate, XED_ICLASS_NOP, 0);
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);
    REQUIRE(c.Mop.decode.is_ctrl == false);
    REQUIRE(c.Mop.decode.is_trap == false);
    REQUIRE(c.Mop.uop[0].decode.is_sta == false);
    REQUIRE(c.Mop.uop[0].decode.is_sta == false);
    REQUIRE(c.Mop.uop[0].decode.is_std == false);
    REQUIRE(c.Mop.uop[0].decode.is_nop == true);
}

TEST_CASE("RR logic", "[uop]") {
    using namespace xiosim::x86;

    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_ADD, 0,
              xed_reg(XED_REG_EAX),
              xed_reg(XED_REG_EDX));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);
    REQUIRE(c.Mop.decode.is_ctrl == false);
    REQUIRE(c.Mop.decode.is_trap == false);
    REQUIRE(c.Mop.uop[0].decode.is_load == false);
    REQUIRE(c.Mop.uop[0].decode.is_sta == false);
    REQUIRE(c.Mop.uop[0].decode.is_std == false);
    REQUIRE(c.Mop.uop[0].decode.is_nop == false);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == XED_REG_EAX);
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == XED_REG_EDX);
    REQUIRE(c.Mop.uop[0].decode.idep_name[2] == XED_REG_INVALID);

    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_EAX);
    REQUIRE(c.Mop.uop[0].decode.odep_name[1] == XED_REG_EFLAGS);
}

TEST_CASE("RR logic 16", "[uop]") {
    using namespace xiosim::x86;

    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_ADD, 16,
              xed_reg(XED_REG_AX),
              xed_reg(XED_REG_DX));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);
    REQUIRE(c.Mop.decode.is_ctrl == false);
    REQUIRE(c.Mop.decode.is_trap == false);
    REQUIRE(c.Mop.uop[0].decode.is_load == false);
    REQUIRE(c.Mop.uop[0].decode.is_sta == false);
    REQUIRE(c.Mop.uop[0].decode.is_std == false);
    REQUIRE(c.Mop.uop[0].decode.is_nop == false);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == XED_REG_EAX);
}

TEST_CASE("jmp ind", "[uop]") {
    using namespace xiosim::x86;

    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_JMP, 0,
              xed_reg(XED_REG_EAX));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);
    REQUIRE(c.Mop.decode.is_ctrl == true);
    REQUIRE(c.Mop.decode.is_trap == false);
    REQUIRE(c.Mop.uop[0].decode.is_ctrl == true);
    REQUIRE(c.Mop.uop[0].decode.is_load == false);
    REQUIRE(c.Mop.uop[0].decode.is_sta == false);
    REQUIRE(c.Mop.uop[0].decode.is_std == false);
    REQUIRE(c.Mop.uop[0].decode.is_nop == false);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == XED_REG_EAX);
}

TEST_CASE("load disp", "[uop]") {
    using namespace xiosim::x86;

    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_MOV, 0,
              xed_reg(XED_REG_EAX),
              xed_mem_gd(XED_REG_DS,
                         xed_disp(0xdeadbeef, 32), 32));

    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);
    REQUIRE(c.Mop.decode.is_ctrl == false);
    REQUIRE(c.Mop.decode.is_trap == false);
    REQUIRE(c.Mop.uop[0].decode.is_load == true);
    REQUIRE(c.Mop.uop[0].decode.is_sta == false);
    REQUIRE(c.Mop.uop[0].decode.is_std == false);
    REQUIRE(c.Mop.uop[0].decode.is_nop == false);
}

TEST_CASE("load base", "[uop]") {
    using namespace xiosim::x86;

    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_MOV, 0,
              xed_reg(XED_REG_EAX),
              xed_mem_b(XED_REG_ESI, 32));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);
    REQUIRE(c.Mop.decode.is_ctrl == false);
    REQUIRE(c.Mop.decode.is_trap == false);
    REQUIRE(c.Mop.uop[0].decode.is_load == true);
    REQUIRE(c.Mop.uop[0].decode.is_sta == false);
    REQUIRE(c.Mop.uop[0].decode.is_std == false);
    REQUIRE(c.Mop.uop[0].decode.is_nop == false);
}

TEST_CASE("store base", "[uop]") {
    using namespace xiosim::x86;

    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_MOV, 0,
              xed_mem_b(XED_REG_ESI, 32),
              xed_reg(XED_REG_EAX));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 2);
    REQUIRE(c.Mop.decode.is_ctrl == false);
    REQUIRE(c.Mop.uop[0].decode.is_sta == true);
    REQUIRE(c.Mop.uop[1].decode.is_std == true);
}

TEST_CASE("load-op", "[uop]") {
    using namespace xiosim::x86;

    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_ADD, 0,
              xed_reg(XED_REG_EDX),
              xed_mem_b(XED_REG_ESI, 32));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 2);
    REQUIRE(c.Mop.uop[0].decode.is_load == true);
    REQUIRE(c.Mop.uop[1].decode.is_load == false);

    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.idep_name[1] == XED_REG_EDX);

    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == XED_REG_EDX);
    REQUIRE(c.Mop.uop[1].decode.odep_name[1] == XED_REG_EFLAGS);
}

TEST_CASE("load-op-store", "[uop]") {
    using namespace xiosim::x86;

    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_ADD, 0,
              xed_mem_b(XED_REG_EDX, 32),
              xed_reg(XED_REG_EAX));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 4);
    REQUIRE(c.Mop.uop[0].decode.is_load == true);
    REQUIRE(c.Mop.uop[1].decode.is_load == false);
    REQUIRE(c.Mop.uop[1].decode.is_sta == false);
    REQUIRE(c.Mop.uop[1].decode.is_std == false);
    REQUIRE(c.Mop.uop[2].decode.is_sta == true);
    REQUIRE(c.Mop.uop[3].decode.is_std == true);

    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_TMP0);

    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.idep_name[1] == XED_REG_EAX);
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == XED_REG_TMP1);
    REQUIRE(c.Mop.uop[1].decode.odep_name[1] == XED_REG_EFLAGS);

    REQUIRE(c.Mop.uop[2].decode.idep_name[0] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[2].decode.odep_name[0] == XED_REG_INVALID);

    REQUIRE(c.Mop.uop[3].decode.idep_name[0] == XED_REG_TMP1);
    REQUIRE(c.Mop.uop[3].decode.idep_name[1] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[3].decode.odep_name[0] == XED_REG_INVALID);
}

TEST_CASE("call", "[uop]") {
    using namespace xiosim::x86;

    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_CALL_NEAR, 0,
              xed_relbr(0xdeadbeef - 5, 32));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 4);
    REQUIRE(c.Mop.uop[0].decode.is_sta == true);
    REQUIRE(c.Mop.uop[1].decode.is_std == true);
    REQUIRE(c.Mop.uop[2].decode.is_load == false);
    REQUIRE(c.Mop.uop[2].decode.is_sta == false);
    REQUIRE(c.Mop.uop[2].decode.is_std == false);
    REQUIRE(c.Mop.uop[3].decode.is_ctrl == true);
}

// TODO: ret tests
// TODO: push and pop tests

TEST_CASE("mul", "[uop]") {
    using namespace xiosim::x86;

    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_MUL, 0,
              xed_reg(XED_REG_ECX));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 3);
    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == XED_REG_ECX);
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == XED_REG_EAX);
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == XED_REG_EAX);
    REQUIRE(c.Mop.uop[2].decode.odep_name[0] == XED_REG_EDX);
}

