VOID IgnoreCallsTo(ADDRINT addr, UINT32 num_insn, ADDRINT replacement_pc);
extern ADDRINT NextUnignoredPC(ADDRINT pc);
extern VOID InstrumentInsIgnoring(TRACE trace, VOID* v);
bool IsInstructionIgnored(ADDRINT pc);
