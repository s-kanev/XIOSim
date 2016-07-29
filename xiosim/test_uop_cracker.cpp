#include "catch.hpp"

#define DECODE_DEBUG
#include "fu.h"
#include "regs.h"
#include "test_xed_context.h"


TEST_CASE("nop", "[uop]") {
    xed_context c;
    xed_inst0(&c.x, c.dstate, XED_ICLASS_NOP, 0);
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);
    REQUIRE(c.Mop.decode.is_ctrl == false);
    REQUIRE(c.Mop.decode.is_trap == false);
    REQUIRE(c.Mop.uop[0].decode.is_load == false);
    REQUIRE(c.Mop.uop[0].decode.is_sta == false);
    REQUIRE(c.Mop.uop[0].decode.is_std == false);
    REQUIRE(c.Mop.uop[0].decode.is_nop == true);
    REQUIRE(c.Mop.uop[0].decode.FU_class == FU_NA);
}

TEST_CASE("multi-byte nop", "[uop]") {
    xed_context c;
    xed_inst0(&c.x, c.dstate, XED_ICLASS_NOP3, 0);
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);
    REQUIRE(c.Mop.decode.is_ctrl == false);
    REQUIRE(c.Mop.decode.is_trap == false);
    REQUIRE(c.Mop.uop[0].decode.is_load == false);
    REQUIRE(c.Mop.uop[0].decode.is_sta == false);
    REQUIRE(c.Mop.uop[0].decode.is_std == false);
    REQUIRE(c.Mop.uop[0].decode.is_nop == true);
}

TEST_CASE("RR logic", "[uop]") {
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

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == largest_reg(XED_REG_EDX));
    REQUIRE(c.Mop.uop[0].decode.idep_name[2] == XED_REG_INVALID);

    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[0].decode.odep_name[1] == largest_reg(XED_REG_EFLAGS));

    REQUIRE(c.Mop.uop[0].decode.FU_class == FU_IEU);
}

TEST_CASE("RR logic 16", "[uop]") {
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

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_EAX));
}

TEST_CASE("jmp ind", "[uop]") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_JMP, c.op_width,
              xed_reg(largest_reg(XED_REG_EAX)));
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

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[0].decode.FU_class == FU_JEU);
}

TEST_CASE("load disp", "[uop]") {
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
    REQUIRE(c.Mop.uop[0].decode.FU_class == FU_LD);
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == largest_reg(XED_REG_EAX));
}

TEST_CASE("load base", "[uop]") {
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
    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ESI));
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == largest_reg(XED_REG_EAX));
}

TEST_CASE("store base", "[uop]") {
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

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ESI));
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[0].decode.FU_class == FU_STA);
    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[1].decode.FU_class == FU_STD);
    REQUIRE(c.Mop.uop[1].decode.fusable.STA_STD == true);
}

TEST_CASE("store base imm", "[uop]") {
    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_MOV, 0,
              xed_mem_b(XED_REG_ESI, 32),
              xed_imm0(0xdecafbad, 32));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 2);
    REQUIRE(c.Mop.decode.is_ctrl == false);
    REQUIRE(c.Mop.uop[0].decode.is_sta == true);
    REQUIRE(c.Mop.uop[1].decode.is_std == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ESI));
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == XED_REG_INVALID);
}

TEST_CASE("load-op", "[uop]") {
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
    REQUIRE(c.Mop.uop[1].decode.idep_name[1] == largest_reg(XED_REG_EDX));

    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == largest_reg(XED_REG_EDX));
    REQUIRE(c.Mop.uop[1].decode.odep_name[1] == largest_reg(XED_REG_EFLAGS));

    REQUIRE(c.Mop.uop[0].decode.FU_class == FU_LD);
    REQUIRE(c.Mop.uop[1].decode.FU_class == FU_IEU);
    REQUIRE(c.Mop.uop[1].decode.fusable.LOAD_OP == true);
}

TEST_CASE("load-op-store", "[uop]") {
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

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_EDX));
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_TMP0);

    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.idep_name[1] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == XED_REG_TMP1);
    REQUIRE(c.Mop.uop[1].decode.odep_name[1] == largest_reg(XED_REG_EFLAGS));

    REQUIRE(c.Mop.uop[2].decode.idep_name[0] == largest_reg(XED_REG_EDX));
    REQUIRE(c.Mop.uop[2].decode.odep_name[0] == XED_REG_INVALID);

    REQUIRE(c.Mop.uop[3].decode.idep_name[0] == XED_REG_TMP1);
    REQUIRE(c.Mop.uop[3].decode.idep_name[1] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[3].decode.odep_name[0] == XED_REG_INVALID);

    REQUIRE(c.Mop.uop[0].decode.FU_class == FU_LD);
    REQUIRE(c.Mop.uop[1].decode.FU_class == FU_IEU);
    REQUIRE(c.Mop.uop[2].decode.FU_class == FU_STA);
    REQUIRE(c.Mop.uop[3].decode.FU_class == FU_STD);

    REQUIRE(c.Mop.uop[1].decode.fusable.LOAD_OP == true);
    REQUIRE(c.Mop.uop[1].decode.fusable.LOAD_OP_ST == true);
    REQUIRE(c.Mop.uop[2].decode.fusable.LOAD_OP_ST == true);
    REQUIRE(c.Mop.uop[3].decode.fusable.STA_STD == true);
    REQUIRE(c.Mop.uop[3].decode.fusable.LOAD_OP_ST == true);
}

TEST_CASE("call", "[uop]") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_CALL_NEAR, 0,
              xed_relbr(0xdeadbeef - 5, 32));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 3);
    REQUIRE(c.Mop.uop[0].decode.is_sta == true);
    REQUIRE(c.Mop.uop[1].decode.is_std == true);
    REQUIRE(c.Mop.uop[2].decode.is_ctrl == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ESP));
    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[2].decode.idep_name[0] == XED_REG_INVALID);

    REQUIRE(c.Mop.uop[0].decode.FU_class == FU_STA);
    REQUIRE(c.Mop.uop[1].decode.FU_class == FU_STD);
    REQUIRE(c.Mop.uop[2].decode.FU_class == FU_JEU);
}

TEST_CASE("call ind reg", "[uop]") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_CALL_NEAR, c.op_width,
              xed_reg(largest_reg(XED_REG_EAX)));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 3);
    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ESP));
    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[2].decode.idep_name[0] == largest_reg(XED_REG_EAX));
}

TEST_CASE("call ind mem", "[uop]") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_CALL_NEAR, c.op_width,
              xed_mem_b(XED_REG_EDX, c.mem_op_width));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 4);
    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ESP));
    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[2].decode.idep_name[0] == largest_reg(XED_REG_EDX));
    REQUIRE(c.Mop.uop[2].decode.odep_name[0] == XED_REG_TMP0);
}

TEST_CASE("ret", "[uop]") {
    xed_context c;
    xed_inst0(&c.x, c.dstate, XED_ICLASS_RET_NEAR, c.op_width);
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 2);
    REQUIRE(c.Mop.uop[0].decode.is_load == true);
    REQUIRE(c.Mop.uop[1].decode.is_ctrl == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ESP));
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_TMP0);

    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.FU_class == FU_JEU);
}

TEST_CASE("push reg", "[uop]") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_PUSH, c.op_width,
              xed_reg(largest_reg(XED_REG_ESI)));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 2);
    REQUIRE(c.Mop.uop[0].decode.is_sta == true);
    REQUIRE(c.Mop.uop[1].decode.is_std == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ESP));
    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == largest_reg(XED_REG_ESI));
}

TEST_CASE("push imm", "[uop]") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_PUSH, c.op_width,
              xed_imm0(0xdeadbeef, 32));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 2);
    REQUIRE(c.Mop.uop[0].decode.is_sta == true);
    REQUIRE(c.Mop.uop[1].decode.is_std == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ESP));
    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == XED_REG_INVALID);
}

TEST_CASE("pushf", "[uop]") {
    xed_context c;
#ifdef _LP64
    xed_inst0(&c.x, c.dstate, XED_ICLASS_PUSHFQ, c.op_width);
#else
    xed_inst0(&c.x, c.dstate, XED_ICLASS_PUSHFD, 0);
#endif
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 2);
    REQUIRE(c.Mop.uop[0].decode.is_sta == true);
    REQUIRE(c.Mop.uop[1].decode.is_std == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ESP));
    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == largest_reg(XED_REG_EFLAGS));
}


TEST_CASE("push mem", "[uop]") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_PUSH, c.op_width,
              xed_mem_b(XED_REG_EDX, c.mem_op_width));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 3);
    REQUIRE(c.Mop.uop[0].decode.is_load == true);
    REQUIRE(c.Mop.uop[1].decode.is_sta == true);
    REQUIRE(c.Mop.uop[2].decode.is_std == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_EDX));
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == largest_reg(XED_REG_ESP));
    REQUIRE(c.Mop.uop[2].decode.idep_name[0] == XED_REG_TMP0);
}

TEST_CASE("pop reg", "[uop]") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_POP, c.op_width,
              xed_reg(largest_reg(XED_REG_ESI)));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);
    REQUIRE(c.Mop.uop[0].decode.is_load == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ESP));
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == largest_reg(XED_REG_ESI));
}

TEST_CASE("pop mem", "[uop]") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_POP, c.op_width,
              xed_mem_b(XED_REG_EDX, c.mem_op_width));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 3);
    REQUIRE(c.Mop.uop[0].decode.is_load == true);
    REQUIRE(c.Mop.uop[1].decode.is_sta == true);
    REQUIRE(c.Mop.uop[2].decode.is_std == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ESP));
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == largest_reg(XED_REG_EDX));
    REQUIRE(c.Mop.uop[2].decode.idep_name[0] == XED_REG_TMP0);
}

TEST_CASE("mul", "[uop]") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_MUL, 0,
              xed_reg(XED_REG_ECX));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 3);
    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ECX));
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_TMP0);

    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[1].decode.idep_name[1] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[1].decode.FU_class == FU_IMUL);

    REQUIRE(c.Mop.uop[2].decode.idep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[2].decode.idep_name[1] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[2].decode.odep_name[0] == largest_reg(XED_REG_EDX));
    REQUIRE(c.Mop.uop[2].decode.FU_class == FU_IMUL);
}

TEST_CASE("imul mem", "[uop]") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_IMUL, 0,
              xed_mem_b(XED_REG_EDX, 32));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 3);
    REQUIRE(c.Mop.uop[0].decode.is_load == true);
    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_EDX));
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_TMP0);

    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[1].decode.idep_name[1] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == largest_reg(XED_REG_EAX));

    REQUIRE(c.Mop.uop[2].decode.idep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[2].decode.idep_name[1] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[2].decode.odep_name[0] == largest_reg(XED_REG_EDX));

    REQUIRE(c.Mop.uop[1].decode.fusable.LOAD_OP == false);
    REQUIRE(c.Mop.uop[2].decode.fusable.LOAD_OP == false);
}

TEST_CASE("imul 3op", "[uop]") {
    xed_context c;
    xed_inst3(&c.x, c.dstate, XED_ICLASS_IMUL, 0,
              xed_reg(XED_REG_EAX),
              xed_mem_b(XED_REG_EDX, 32),
              xed_imm0(0xdeadbeef, 32));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 2);
    REQUIRE(c.Mop.uop[0].decode.is_load == true);
    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_EDX));
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_TMP0);

    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[1].decode.fusable.LOAD_OP == true);
}

TEST_CASE("div", "[uop]") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_DIV, 0,
              xed_reg(XED_REG_ECX));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 3);
    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ECX));
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_TMP0);

    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[1].decode.idep_name[1] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[1].decode.FU_class == FU_IDIV);

    REQUIRE(c.Mop.uop[2].decode.idep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[2].decode.idep_name[1] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[2].decode.odep_name[0] == largest_reg(XED_REG_EDX));
    REQUIRE(c.Mop.uop[1].decode.FU_class == FU_IDIV);

    REQUIRE(c.Mop.uop[1].decode.fusable.LOAD_OP == false);
    REQUIRE(c.Mop.uop[2].decode.fusable.LOAD_OP == false);
}


TEST_CASE("je", "[uop]") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_JZ, 0,
              xed_relbr(0xdeadbeef - 6, 32));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);
    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_EFLAGS));
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[0].decode.idep_name[2] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[0].decode.FU_class == FU_JEU);
}

TEST_CASE("lea", "[uop]") {
    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_LEA, 0,
              xed_reg(XED_REG_ESI),
              xed_mem_bd(XED_REG_EDX,
                         xed_disp(0xbeef, 32),
                         c.mem_op_width));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);
    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_EDX));
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[0].decode.idep_name[2] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == largest_reg(XED_REG_ESI));
    REQUIRE(c.Mop.uop[0].decode.odep_name[1] == XED_REG_INVALID);

    REQUIRE(c.Mop.uop[0].decode.is_sta == false);
    REQUIRE(c.Mop.uop[0].decode.is_std == false);
    REQUIRE(c.Mop.uop[0].decode.is_load == false);
    REQUIRE(c.Mop.uop[0].decode.is_agen == true);
    REQUIRE(c.Mop.uop[0].decode.FU_class == FU_AGEN);
}

TEST_CASE("inc load-op-store", "[uop]") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_INC, c.op_width,
              xed_mem_bd(XED_REG_EDX,
                         xed_disp(0xbeef, 32),
                         c.mem_op_width));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 4);
    REQUIRE(c.Mop.uop[0].decode.is_load == true);
    REQUIRE(c.Mop.uop[1].decode.is_load == false);
    REQUIRE(c.Mop.uop[1].decode.is_sta == false);
    REQUIRE(c.Mop.uop[1].decode.is_std == false);
    REQUIRE(c.Mop.uop[2].decode.is_sta == true);
    REQUIRE(c.Mop.uop[3].decode.is_std == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_EDX));
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_TMP0);

    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == XED_REG_TMP1);
    REQUIRE(c.Mop.uop[1].decode.odep_name[1] == largest_reg(XED_REG_EFLAGS));

    REQUIRE(c.Mop.uop[2].decode.idep_name[0] == largest_reg(XED_REG_EDX));
    REQUIRE(c.Mop.uop[2].decode.odep_name[0] == XED_REG_INVALID);

    REQUIRE(c.Mop.uop[3].decode.idep_name[0] == XED_REG_TMP1);
    REQUIRE(c.Mop.uop[3].decode.idep_name[1] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[3].decode.odep_name[0] == XED_REG_INVALID);
}

TEST_CASE("stos", "[uop]") {
    xed_context c;
    xed_inst0(&c.x, c.dstate, XED_ICLASS_STOSD, 0);
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 2);
    REQUIRE(c.Mop.decode.is_ctrl == false);
    REQUIRE(c.Mop.uop[0].decode.is_sta == true);
    REQUIRE(c.Mop.uop[1].decode.is_std == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_EDI));
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == XED_REG_INVALID);
}

TEST_CASE("lods", "[uop]") {
    xed_context c;
    xed_inst0(&c.x, c.dstate, XED_ICLASS_LODSD, 0);
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);
    REQUIRE(c.Mop.uop[0].decode.is_load == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ESI));
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == largest_reg(XED_REG_EAX));
}

TEST_CASE("movs", "[uop]") {
    xed_context c;
    xed_inst0(&c.x, c.dstate, XED_ICLASS_MOVSD, 0);
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 3);
    REQUIRE(c.Mop.uop[0].decode.is_load == true);
    REQUIRE(c.Mop.uop[1].decode.is_sta == true);
    REQUIRE(c.Mop.uop[2].decode.is_std == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ESI));
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == largest_reg(XED_REG_EDI));
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == XED_REG_INVALID);
    REQUIRE(c.Mop.uop[2].decode.idep_name[0] == XED_REG_TMP0);
}

TEST_CASE("scas", "[uop]") {
    xed_context c;
    xed_inst0(&c.x, c.dstate, XED_ICLASS_SCASD, 0);
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 2);
    REQUIRE(c.Mop.uop[0].decode.is_load == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_EDI));
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_TMP0);

    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.idep_name[1] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == largest_reg(XED_REG_EFLAGS));
}

TEST_CASE("cmps", "[uop]") {
    xed_context c;
    xed_inst0(&c.x, c.dstate, XED_ICLASS_CMPSD, 0);
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 3);
    REQUIRE(c.Mop.uop[0].decode.is_load == true);
    REQUIRE(c.Mop.uop[1].decode.is_load == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ESI));
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == largest_reg(XED_REG_EDI));
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == XED_REG_TMP1);

    REQUIRE(c.Mop.uop[2].decode.idep_name[0] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[2].decode.idep_name[1] == XED_REG_TMP1);
    REQUIRE(c.Mop.uop[2].decode.odep_name[0] == largest_reg(XED_REG_EFLAGS));
}

TEST_CASE("RR shift", "[uop]") {
    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_SAR, 0,
              xed_reg(XED_REG_EAX),
              xed_reg(XED_REG_CL));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == largest_reg(XED_REG_ECX));

    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[0].decode.odep_name[1] == largest_reg(XED_REG_EFLAGS));

    REQUIRE(c.Mop.uop[0].decode.FU_class == FU_SHIFT);
}

TEST_CASE("RR shiftx", "[uop]") {
    xed_context c;
    xed_inst3(&c.x, c.dstate, XED_ICLASS_SARX, 0,
              xed_reg(XED_REG_EAX),
              xed_reg(XED_REG_EDX),
              xed_reg(XED_REG_ECX));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_EDX));
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == largest_reg(XED_REG_ECX));

    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[0].decode.odep_name[1] == XED_REG_INVALID);

    REQUIRE(c.Mop.uop[0].decode.FU_class == FU_SHIFT);
}

TEST_CASE("RR FP add", "[uop]") {
    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_ADDPD, 0,
              xed_reg(XED_REG_XMM1),
              xed_reg(XED_REG_XMM2));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);
    REQUIRE(c.Mop.uop[0].decode.is_fpop == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_XMM1));
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == largest_reg(XED_REG_XMM2));
    REQUIRE(c.Mop.uop[0].decode.idep_name[2] == XED_REG_INVALID);

    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == largest_reg(XED_REG_XMM1));
    REQUIRE(c.Mop.uop[0].decode.odep_name[1] == XED_REG_INVALID);

    REQUIRE(c.Mop.uop[0].decode.FU_class == FU_FADD);
}

TEST_CASE("Mem FP add", "[uop]") {
    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_ADDPD, 0,
              xed_reg(XED_REG_XMM1),
              xed_mem_bd(XED_REG_EDX,
                         xed_disp(0xbeef, 32),
                         128));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 2);
    REQUIRE(c.Mop.uop[0].decode.is_load == true);
    REQUIRE(c.Mop.uop[1].decode.is_fpop == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_EDX));
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_TMP0);

    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.idep_name[1] == largest_reg(XED_REG_XMM1));
    REQUIRE(c.Mop.uop[1].decode.idep_name[2] == XED_REG_INVALID);

    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == largest_reg(XED_REG_XMM1));
    REQUIRE(c.Mop.uop[1].decode.odep_name[1] == XED_REG_INVALID);

    REQUIRE(c.Mop.uop[0].decode.FU_class == FU_LD);
    REQUIRE(c.Mop.uop[1].decode.FU_class == FU_FADD);

    REQUIRE(c.Mop.uop[1].decode.fusable.FP_LOAD_OP == true);
}

TEST_CASE("RR padd", "[uop]") {
    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_PADDD, 0,
              xed_reg(XED_REG_XMM1),
              xed_reg(XED_REG_XMM2));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);
    REQUIRE(c.Mop.uop[0].decode.is_fpop == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_XMM1));
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == largest_reg(XED_REG_XMM2));

    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == largest_reg(XED_REG_XMM1));

    /* Parallel int ops for now go to the integer unit */
    REQUIRE(c.Mop.uop[0].decode.FU_class == FU_IEU);
}

TEST_CASE("MOVSD_XMM", "[uop]") {
    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_MOVSD_XMM, 0,
              xed_reg(XED_REG_XMM1),
              xed_mem_bd(XED_REG_EDX,
                         xed_disp(0xbeef, 32),
                         64));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);
    REQUIRE(c.Mop.uop[0].decode.is_load == true);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_EDX));
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == largest_reg(XED_REG_XMM1));
}

TEST_CASE("FSINCOS", "[uop]") {
    xed_context c;
    /* XXX: fsincons is actually 0-operand. Why does XED insist on 2 explicit ones?? */
    xed_inst2(&c.x, c.dstate, XED_ICLASS_FSINCOS, 0,
              xed_reg(XED_REG_ST0),
              xed_reg(XED_REG_ST1));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 2);

    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == largest_reg(XED_REG_ST1));
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == largest_reg(XED_REG_ST0));
}

TEST_CASE("CMOVcc", "[uop]") {
    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_CMOVZ, 0,
              xed_reg(XED_REG_EAX),
              xed_reg(XED_REG_EBX));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 2);

    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_EFLAGS));
    REQUIRE(c.Mop.uop[0].decode.idep_name[1] == largest_reg(XED_REG_EBX));
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == largest_reg(XED_REG_EAX));
}

TEST_CASE("CMPXCHG", "[uop]") {
    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_CMPXCHG, 0,
              xed_mem_bd(XED_REG_EDX,
                         xed_disp(0xbeef, 32),
                         32),
              xed_reg(XED_REG_ECX));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 10);
}

TEST_CASE("lock CMPXCHG", "[uop]") {
    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_CMPXCHG, 0,
              xed_mem_bd(XED_REG_EDX,
                         xed_disp(0xbeef, 32),
                         32),
              xed_reg(XED_REG_ECX));
    xed_lock(&c.x);
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 14);
}

TEST_CASE("XCHG RR", "[uop]") {
    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_XCHG, 0,
              xed_reg(XED_REG_ESI),
              xed_reg(XED_REG_ECX));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 3);
    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ECX));
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == largest_reg(XED_REG_ESI));
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == largest_reg(XED_REG_ECX));
    REQUIRE(c.Mop.uop[2].decode.idep_name[0] == XED_REG_TMP0);
    REQUIRE(c.Mop.uop[2].decode.odep_name[0] == largest_reg(XED_REG_ESI));
}

TEST_CASE("XCHG", "[uop]") {
    xed_context c;
    /* XXX: implicit lock prefix */
    xed_inst2(&c.x, c.dstate, XED_ICLASS_XCHG, 0,
              xed_mem_bd(XED_REG_EDX,
                         xed_disp(0xbeef, 32),
                         32),
              xed_reg(XED_REG_ECX));
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 8);
}

TEST_CASE("PREFETCHT0", "[uop]") {
    xed_context c;
    xed_inst1(&c.x, c.dstate, XED_ICLASS_PREFETCHT0, 0,
              xed_mem_bd(largest_reg(XED_REG_EDX),
                         xed_disp(0xbeef, 32),
                         8 * 64));  // get that whole cache line!
    c.encode();

    c.decode_and_crack();

    REQUIRE(c.Mop.decode.flow_length == 1);
    REQUIRE(c.Mop.uop[0].decode.is_load);
    REQUIRE(c.Mop.uop[0].decode.is_pf);
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == XED_REG_INVALID);
}

TEST_CASE("MAGIC BLSR", "[magic blsr]") {
    xed_context c;
    xed_inst2(&c.x, c.dstate, XED_ICLASS_BLSR, 0,
              xed_reg(largest_reg(XED_REG_EAX)),
              xed_reg(largest_reg(XED_REG_ECX)));
    c.Mop.decode.is_magic = true;
    c.encode();
    c.decode_and_crack();

    REQUIRE(c.Mop.decode.is_magic == true);
    REQUIRE(c.Mop.decode.flow_length == 2);
    REQUIRE(c.Mop.uop[0].decode.FU_class == FU_SIZE_CLASS);
    REQUIRE(c.Mop.uop[1].decode.FU_class == FU_IEU);
    REQUIRE(c.Mop.uop[0].decode.idep_name[0] == largest_reg(XED_REG_ECX));
    REQUIRE(c.Mop.uop[1].decode.idep_name[0] == largest_reg(XED_REG_ECX));
    REQUIRE(c.Mop.uop[0].decode.odep_name[0] == largest_reg(XED_REG_EAX));
    REQUIRE(c.Mop.uop[1].decode.odep_name[0] == largest_reg(XED_REG_ECX));
    REQUIRE(c.Mop.uop[1].decode.odep_name[1] == largest_reg(XED_REG_EFLAGS));
}
