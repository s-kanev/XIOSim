#ifndef __IGNORE_INS__
#define __IGNORE_INS__

#include <string>

#include "feeder.h"

/* Ignore calls and @num_insn (including the call) to the routine at @addr.
 * Looks for @num_insn-1 stack writes.
 * if @replacement_pc != -1, it is used to properly set NPC for the last
 * non-ignored instruction in case the routine is replaced in analysis.
 * XXX: We only ignore direct calls.
 */
VOID IgnoreCallsTo(ADDRINT addr, UINT32 num_insn, ADDRINT replacement_pc);

/* Ignore instances of instruction at address @pc. Will set the NPC to the
 * fallthrough, so be careful if you ignore things like often taken branches
 */
VOID IgnorePC(ADDRINT pc);

/* Add the absolute addresses of ignored instructions. If the PCs are specified
 * through symbols and offsets, then resolve the absolute address.
 */
VOID AddIgnoredInstructionPCs(IMG img, std::vector<std::string>& ignored_pcs);

/* Used at analysis time to properly set NPCs of instructions, taking into
 * account ignored ones.
 */
ADDRINT NextUnignoredPC(ADDRINT pc);

/* Trace-level instrumentation that looks for calls to ignored routines and
 * sets up the static instructions to ignore.
 */
VOID InstrumentInsIgnoring(TRACE trace, VOID* v);

/* At analysis time, check whether the instruction at @pc should be ignored.
 */
bool IsInstructionIgnored(ADDRINT pc);


extern KNOB<string> KnobIgnorePCs;

#endif /* __IGNORE_INS__ */
