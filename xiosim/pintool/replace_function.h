#ifndef __REPLACE_FUNCTION__
#define __REPLACE_FUNCTION__

#include <string>
#include <list>
#include <vector>

#include "feeder.h"
#include "handshake_container.h"

/* Replace @function_name with the xed-encoded instructions supplied in @replacement.
 * @num_params is used to ignore the stack-pushing instructions before the actual
 * calls to @function_name that setup parameters (they will be ignored *after* the
 * first call).
 * The instructions inserted instead of @function_name will have flags.real == false,
 * so that the timing simulator can treat them differently, if it so choses.
 *
 * Here's a sample use-case that replaces a function with just one nop:
    xed_encoder_instruction_t nop;
    xed_inst0(&nop, dstate, XED_ICLASS_NOP, 0);
    list<xed_encoder_instruction_t> insts = {nop};
    AddFunctionReplacement("fib_repl", 1, insts);

 * TODO(skanev): This is abstracted from the helix use-case in ildjit.cpp.
 * Port the helix case to actually use this API.
 */

void AddFunctionReplacement(std::string function_name,
                    size_t num_params,
                    std::list<xed_encoder_instruction_t> replacement);

/* Similar to above, but ignoring individual instructions. */
void AddInstructionReplacement(INS ins,
                    std::list<xed_encoder_instruction_t> replacement);

/* Default case for AddFunctionReplacement that simply replaces @function_name and
 * 0 parameters with a single nop. Useful for testing. */
void IgnoreFunction(std::string function_name);
extern KNOB<std::string> KnobIgnoreFunctions;

/* Adds instrumentation to ignore the taken path of @jcc.
 * We have to be a bit careful with this, because it just sets a global flag and
 * stopping to ignore is done at the exit points of the RTN containing the jump
 * (we don't know what's the end of a taken path).*/
void IgnoreTakenBranchPath(INS jcc);
/* Adds instrumentation on the exit paths of @rtn that stops ignoring taken branch paths.
 * Assumes @rtn is already open. */
void StopIgnoringTakenBranch(RTN rtn);

/* Similar to IgnoreTakenBranch, ignore the dynamic sequence of insns between bounds[0]
 * and bounds[1]. */
void IgnoreBetween(const std::vector<INS>& bounds);

/* Add @insns for simulation on the non-taken path of @jcc *without* ignoring @jcc. */
void AddFallthroughInstructions(INS jcc, std::list<xed_encoder_instruction_t> insns);
/* If we've added magic instructions after the one at @pc, return the pc of the first
 * magic one, as if it were the real ft instruction.
 * If not, return @ftPC. */
ADDRINT GetFixedUpftPC(ADDRINT pc, ADDRINT ftPC);

#endif /* __REPLACE_FUNCTION__ */
