#ifndef _XED_UTILS_H_
#define _XED_UTILS_H_

#include <cstdlib>

extern "C" {
#include "xed-interface.h"
}

/* Xed machine mode state for when we need to encode/decode things. */
extern xed_state_t dstate;
#ifdef _LP64
const size_t xed_mem_op_width = 64;
#else
const size_t xed_mem_op_width = 32;
#endif

/* Initialize XED. */
void InitXed();

/* Encode an instruction into the provided buffer and return the length. */
size_t Encode(xed_encoder_instruction_t inst, uint8_t* inst_bytes);

/* Helper to check if a xed-encoded instruction has a memory operand. */
bool XedEncHasMemoryOperand(const xed_encoder_instruction_t& inst);

#endif
