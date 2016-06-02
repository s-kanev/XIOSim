#include <iostream>

#include "feeder.h"
#include "xed_utils.h"

xed_state_t dstate;

void InitXed() {
    xed_tables_init();
#ifdef _LP64
    xed_state_init2(&dstate, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b);
#else
    xed_state_init2(&dstate, XED_MACHINE_MODE_LONG_COMPAT_32, XED_ADDRESS_WIDTH_32b);
#endif
}

size_t Encode(xed_encoder_instruction_t inst, uint8_t* inst_bytes) {
    xed_encoder_request_t enc_req;

    xed_encoder_request_zero_set_mode(&enc_req, &dstate);

    bool convert_ok = xed_convert_to_encoder_request(&enc_req, &inst);
    if (!convert_ok) {
        std::cerr << "conversion to encode request failed" << std::endl;
        PIN_ExitProcess(EXIT_FAILURE);
    }

    unsigned int inst_len;
    auto err = xed_encode(&enc_req, inst_bytes, xiosim::x86::MAX_ILEN, &inst_len);
    if (err != XED_ERROR_NONE) {
        std::cerr << "xed_encode failed " << xed_error_enum_t2str(err) << std::endl;
        PIN_ExitProcess(EXIT_FAILURE);
    }
    return inst_len;
}

/* Returns true if the instruction has a memory operand and will actually access
 * memory using it.
 */
bool XedEncHasMemoryOperand(const xed_encoder_instruction_t& inst) {
    /* LEA instructions have a memory operand but don't actually touch memory. */
    if (inst.iclass == XED_ICLASS_LEA)
        return false;

    for (size_t i = 0; i < inst.noperands; i++)
        if (inst.operands[i].type == XED_ENCODER_OPERAND_TYPE_MEM)
            return true;

    return false;
}
