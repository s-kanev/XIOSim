#ifndef __REPLACE_FUNCTION__
#define __REPLACE_FUNCTION__

#include <string>
#include <list>

#include "feeder.h"
#include "handshake_container.h"

/* Replace @function_name with the xed-encoded instructions supplied in @replacement.
 * @num_params is used to ignore the stack-pushing instructions before the actual
 * calls to @function_name that setup parameters (they will be ignored *after* the
 * first call).
 * The instructions inserted instead of @function_name will have flags.real == false,
 * so that the timing simulator can treat them differently, if it sp choses.
 *
 * Here's a sample use-case that replaces a function with just one nop:
    xed_encoder_instruction_t nop;
    xed_inst0(&nop, dstate, XED_ICLASS_NOP, 0);
    list<xed_encoder_instruction_t> insts = {nop};
    AddReplacement("fib_repl", 1, insts);

 * TODO(skanev): This is abstracted from the helix use-case in ildjit.cpp.
 * Port the helix case to actually use this API.
 */

void AddReplacement(std::string function_name, size_t num_params, std::list<xed_encoder_instruction_t> replacement);

#endif /* __REPLACE_FUNCTION__ */
