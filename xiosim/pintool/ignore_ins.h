/* Ignore calls and @num_insn (including the call) to the routine at @addr.
 * Looks for @num_insn-1 stack writes.
 * if @replacement_pc != -1, it is used to properly set NPC for the last
 * non-ignored instruction in case the routine is replaced in analysis.
 * XXX: We only ignore direct calls.
 */
VOID IgnoreCallsTo(ADDRINT addr, UINT32 num_insn, ADDRINT replacement_pc);

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
