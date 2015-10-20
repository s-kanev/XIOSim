#ifndef __TEST_XED_CONTEXT_H__
#define __TEST_XED_CONTEXT_H__

extern "C" {
#include "xed-interface.h"
}

#include "../decode.h"
#include "../uop_cracker.h"
#include "../zesto-structs.h"

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
        auto err = xed_encode(&enc_req, Mop.fetch.code, MAX_ILEN, &inst_len);
        if (err != XED_ERROR_NONE) {
            fatal("xed_encode failed %s\n", xed_error_enum_t2str(err));
        }
    }

    void decode(void) {
        xiosim::x86::decode(&Mop);
        xiosim::x86::decode_flags(&Mop);

#ifdef DECODE_DEBUG
        std::cerr << print_Mop(&Mop) << std::endl;
#endif
    }

    void decode_and_crack(void) {
        decode();
        xiosim::x86::crack(&Mop);
    }
};

#endif /* __TEST_XED_CONTEXT_H__ */
