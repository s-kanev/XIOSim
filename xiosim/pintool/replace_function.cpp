#include <mutex>

#include "BufferManagerProducer.h"
#include "ignore_ins.h"
#include "speculation.h"
#include "xiosim/synchronization.h"

#include "replace_function.h"
#include "xed_utils.h"

KNOB<string> KnobIgnoreFunctions(KNOB_MODE_WRITEONCE,
                                 "pintool",
                                 "ignore_functions",
                                 "",
                                 "Comma-separated list of functions to replace with a nop");

extern KNOB<BOOL> KnobIgnoringInstructions;

using namespace std;

struct fake_inst_info_t {
    fake_inst_info_t(ADDRINT _pc, size_t _len)
        : pc(_pc)
        , len(_len)
        , has_mem_op(false) {}
    bool operator==(const fake_inst_info_t& rhs) { return pc == rhs.pc && len == rhs.len; }

    ADDRINT pc;
    size_t len;

    bool has_mem_op;
};

/* Metadata for replaced instructions that we need to preserve until analysis time. */
using FakeInstructionsPerPC = std::map<ADDRINT, std::vector<fake_inst_info_t>>;
/* Replacements are a sequence of instructions that will be simulated *instead of*
 * the original instruction at @pc. Inserts will be simulated *in addition to* it. */
static FakeInstructionsPerPC replacements;
static FakeInstructionsPerPC inserts;
static XIOSIM_LOCK replacements_lk;

static bool IsReplaced(ADDRINT pc) {
    std::lock_guard<XIOSIM_LOCK> l(replacements_lk);
    return replacements.find(pc) != replacements.end();
}

static void MarkReplaced(ADDRINT pc, std::vector<fake_inst_info_t> arg) {
    std::lock_guard<XIOSIM_LOCK> l(replacements_lk);
    replacements[pc] = arg;
}

static std::vector<fake_inst_info_t>& GetReplacement(ADDRINT pc) {
    std::lock_guard<XIOSIM_LOCK> l(replacements_lk);
    return replacements[pc];
}

static bool IsInserted(ADDRINT pc) {
    std::lock_guard<XIOSIM_LOCK> l(replacements_lk);
    return inserts.find(pc) != inserts.end();
}

static void MarkInserted(ADDRINT pc, std::vector<fake_inst_info_t> arg) {
    std::lock_guard<XIOSIM_LOCK> l(replacements_lk);
    inserts[pc] = arg;
}

static std::vector<fake_inst_info_t>& GetInsert(ADDRINT pc) {
    std::lock_guard<XIOSIM_LOCK> l(replacements_lk);
    return inserts[pc];
}

/* Map from a PC to its ftPC, in case we've changed it (i.e. added instructions after @PC). */
static map<ADDRINT, ADDRINT> ftPCs;
static XIOSIM_LOCK ftPCs_lk;

static void UpdateftPC(ADDRINT pc, ADDRINT tpc) {
    std::lock_guard<XIOSIM_LOCK> l(ftPCs_lk);
    ftPCs[pc] = tpc;
}

ADDRINT GetFixedUpftPC(ADDRINT pc, ADDRINT ftPC) {
    std::lock_guard<XIOSIM_LOCK> l(ftPCs_lk);
    if (ftPCs.find(pc) == ftPCs.end())
        return ftPC;

    return ftPCs.at(pc);
}

static void AddMagicInstructions(THREADID tid, ADDRINT pc, ADDRINT ftPC, std::vector<fake_inst_info_t>& magic_insns) {
    thread_state_t* tstate = get_tls(tid);

    size_t mem_ind = 0;
    for (auto& inst : magic_insns) {
        bool last_inst = (inst == magic_insns.back());
        handshake_container_t* handshake = xiosim::buffer_management::GetBuffer(tstate->tid);

        handshake->asid = asid;
        handshake->flags.valid = true;
        handshake->flags.real = false;
        handshake->flags.isFirstInsn = false;
        handshake->flags.speculative = speculation_mode;

        handshake->pc = inst.pc;
        handshake->tpc = inst.pc + inst.len;
        handshake->npc = last_inst ? NextUnignoredPC(ftPC) : inst.pc + inst.len;
        handshake->flags.brtaken = false;
        memcpy(handshake->ins, (void*)inst.pc, inst.len);

        /* If the replacement instruction touches memory, we expect that a previous
         * analysis routine has set up the address and stored it in tstate. */
        if (inst.has_mem_op) {
            auto mem_op = tstate->replacement_mem_ops.at(pc).at(mem_ind);
            handshake->mem_buffer.push_back(mem_op);
            mem_ind++;
        }

#ifdef PRINT_DYN_TRACE
        printTrace("sim", handshake->pc, tid);
#endif

        xiosim::buffer_management::ProducerDone(tstate->tid);
    }
}

static void StartIgnoring(THREADID tid) {
    thread_state_t* tstate = get_tls(tid);
    std::lock_guard<XIOSIM_LOCK> l(tstate->lock);
    tstate->ignore = true;
}

static void StopIgnoring(THREADID tid) {
    thread_state_t* tstate = get_tls(tid);
    std::lock_guard<XIOSIM_LOCK> l(tstate->lock);
    tstate->ignore = false;
}

/* Start ignoring instruction before the replaced instruction, and add the magically-encoded
 * replacement instructions to the producer buffers. */
static void ReplacedBefore(THREADID tid, ADDRINT pc, ADDRINT ftPC) {
    StartIgnoring(tid);

    auto repl_state = GetReplacement(pc);
    AddMagicInstructions(tid, pc, ftPC, repl_state);
}

/* Stop ignoring instruction after the replaced instruction. */
static void ReplacedAfter(THREADID tid) { StopIgnoring(tid); }

/* Just add the encoded inserts to producer buffers. */
static void InsertedBefore(THREADID tid, ADDRINT pc, ADDRINT ftPC) {
    auto magic_insns = GetInsert(pc);
    AddMagicInstructions(tid, pc, ftPC, magic_insns);
}

/* Helper struct to send parameters to image instrumentation routines. */
struct replacement_params_t {
    string function_name;
    size_t num_params;
    list<xed_encoder_instruction_t> insts;
};

static std::vector<fake_inst_info_t> PrepareReplacementBuffer(
        const std::list<xed_encoder_instruction_t>& insts) {
    std::vector<fake_inst_info_t> encoded_insts;
    if (insts.empty())
        return encoded_insts;

    /* Allocate space for the replacement instructions so they have real PCs. */
    void* inst_buffer = malloc(insts.size() * xiosim::x86::MAX_ILEN);
    if (inst_buffer == nullptr) {
        cerr << "Failed to alloc inst buffer. " << endl;
        PIN_ExitProcess(EXIT_FAILURE);
    }

    /* Encode replacement instructions and place them in inst_buffer. */
    uint8_t* curr_buffer = static_cast<uint8_t*>(inst_buffer);
    for (auto& x : insts) {
        size_t n_bytes = Encode(x, curr_buffer);
        fake_inst_info_t new_inst((ADDRINT)curr_buffer, n_bytes);
        if (XedEncHasMemoryOperand(x))
            new_inst.has_mem_op = true;
        encoded_insts.push_back(new_inst);
        curr_buffer += n_bytes;
    }
    return encoded_insts;
}

static ADDRINT GetFirstPC(const std::vector<fake_inst_info_t>& encoded_insts) {
    ADDRINT alt_pc = static_cast<ADDRINT>(-1);
    if (encoded_insts.size())
        alt_pc = encoded_insts.front().pc;
    return alt_pc;
}

static void AddReplacementCalls(IMG img, void* v) {
    replacement_params_t* params = static_cast<replacement_params_t*>(v);
    RTN rtn = RTN_FindByName(img, params->function_name.c_str());
    if (RTN_Valid(rtn)) {
        ADDRINT rtn_pc = RTN_Address(rtn);
        if (IsReplaced(rtn_pc))
            return;

        auto encoded_insts = PrepareReplacementBuffer(params->insts);

        /* Add the actual instrumentation callbacks. */
        RTN_Open(rtn);
        RTN_InsertCall(rtn,
                       IPOINT_BEFORE,
                       AFUNPTR(ReplacedBefore),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_RETURN_IP,
                       IARG_CALL_ORDER,
                       CALL_ORDER_FIRST,
                       IARG_END);
        RTN_InsertCall(rtn,
                       IPOINT_AFTER,
                       AFUNPTR(ReplacedAfter),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER,
                       CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);

        /* Make sure ignoring API is enabled, otherwise below does nothing. */
        ASSERTX(KnobIgnoringInstructions.Value());
        /* Fixup next PC in instrumentation. */
        ADDRINT alt_pc = GetFirstPC(encoded_insts);
        IgnoreCallsTo(rtn_pc, params->num_params + 1 /* the call + param pushes */, alt_pc);

        MarkReplaced(rtn_pc, encoded_insts);
    }
}

static bool PredicateTaken(BOOL taken) { return taken; }

void AddInstructionReplacement(INS ins, std::list<xed_encoder_instruction_t> insts) {
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;

    ADDRINT pc = INS_Address(ins);

    /* This can be true if Pin discovers the instruction twice, say, coming from the
     * two sides of a branch. We still want to add the instrumentation calls, but shouldn't
     * touch anything else that can affect state. */
    if (!IsReplaced(pc)) {
        auto encoded_insts = PrepareReplacementBuffer(insts);
        MarkReplaced(pc, encoded_insts);

        /* Make sure ignoring API is enabled, otherwise below does nothing. */
        ASSERTX(KnobIgnoringInstructions.Value());
        /* Fixup next PC in instrumentation. */
        ADDRINT alt_pc = GetFirstPC(encoded_insts);
        IgnorePC(pc, alt_pc);
    }

    INS_InsertCall(ins,
                   IPOINT_BEFORE,
                   AFUNPTR(ReplacedBefore),
                   IARG_THREAD_ID,
                   IARG_INST_PTR,
                   IARG_FALLTHROUGH_ADDR,
                   IARG_CALL_ORDER,
                   CALL_ORDER_FIRST + 2,  // +2 to order wrt to eventual emulation calls
                   IARG_END);
    INS_InsertCall(ins,
                   IPOINT_AFTER,
                   AFUNPTR(ReplacedAfter),
                   IARG_THREAD_ID,
                   IARG_CALL_ORDER,
                   CALL_ORDER_LAST,
                   IARG_END);

    /* If we're replacing a branch or a call, make sure we stop ignoring on the taken
     * path (the above IPOINT_AFTER call is only valid on the fallthrough). */
    if (INS_IsDirectBranchOrCall(ins)) {
        INS_InsertIfCall(ins,
                         IPOINT_BEFORE,
                         AFUNPTR(PredicateTaken),
                         IARG_BRANCH_TAKEN,
                         IARG_CALL_ORDER,
                         CALL_ORDER_LAST,
                         IARG_END);
        INS_InsertThenCall(ins,
                           IPOINT_BEFORE,
                           AFUNPTR(ReplacedAfter),
                           IARG_THREAD_ID,
                           IARG_CALL_ORDER,
                           CALL_ORDER_LAST,
                           IARG_END);
    }
}

static void StartIgnoringTaken(THREADID tid) {
    thread_state_t* tstate = get_tls(tid);
    std::lock_guard<XIOSIM_LOCK> l(tstate->lock);
    tstate->ignore = true;
    tstate->ignore_taken = true;
}

static void StopIgnoringTaken(THREADID tid) {
    thread_state_t* tstate = get_tls(tid);
    std::lock_guard<XIOSIM_LOCK> l(tstate->lock);
    if (tstate->ignore && tstate->ignore_taken) {
        tstate->ignore = false;
        tstate->ignore_taken = false;
    }
}

void IgnoreTakenBranchPath(INS jcc) {
    INS_InsertIfCall(jcc,
                     IPOINT_BEFORE,
                     AFUNPTR(PredicateTaken),
                     IARG_BRANCH_TAKEN,
                     IARG_END);
    INS_InsertThenCall(jcc,
                       IPOINT_BEFORE,
                       AFUNPTR(StartIgnoringTaken),
                       IARG_THREAD_ID,
                       IARG_END);
}

void StopIgnoringTakenBranch(RTN rtn) {
    RTN_InsertCall(rtn,
                   IPOINT_AFTER,
                   AFUNPTR(StopIgnoringTaken),
                   IARG_THREAD_ID,
                   IARG_END);
}

void AddFallthroughInstructions(INS jcc, list<xed_encoder_instruction_t> insts) {
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;

    ADDRINT pc = INS_Address(jcc);
    if (!IsInserted(pc)) {
        auto encoded_insts = PrepareReplacementBuffer(insts);
        MarkInserted(pc, encoded_insts);

        /* Fixup next PC in instrumentation. */
        ADDRINT alt_pc = GetFirstPC(encoded_insts);
        UpdateftPC(pc, alt_pc);
    }

    INS_InsertCall(jcc,
                   IPOINT_AFTER,  // AFTER on a branch means fallthrough only
                   AFUNPTR(InsertedBefore),
                   IARG_THREAD_ID,
                   IARG_INST_PTR,
                   IARG_FALLTHROUGH_ADDR,
                   IARG_CALL_ORDER,
                   CALL_ORDER_LAST,  // so we add instructions after all jump instrumentation
                   IARG_END);
}

void AddFunctionReplacement(
        string function_name, size_t num_params, list<xed_encoder_instruction_t> insts) {
    replacement_params_t* params = new replacement_params_t();
    params->function_name = function_name;
    params->num_params = num_params;
    params->insts = insts;
    IMG_AddInstrumentFunction(AddReplacementCalls, (void*)params);
}

void IgnoreFunction(string function_name) {
    xed_encoder_instruction_t nop;
    xed_inst0(&nop, dstate, XED_ICLASS_NOP, 0);
    list<xed_encoder_instruction_t> insts = { nop };
    AddFunctionReplacement(function_name, 0, insts);
}
