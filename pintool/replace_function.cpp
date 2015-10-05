#include "replace_function.h"

#include "BufferManagerProducer.h"
#include "ignore_ins.h"
#include "speculation.h"

using namespace std;

struct fake_inst_info_t {
    fake_inst_info_t(ADDRINT _pc, size_t _len)
        : pc(_pc)
        , len(_len) {}
    bool operator==(const fake_inst_info_t& rhs) { return pc == rhs.pc && len == rhs.len; }

    ADDRINT pc;
    size_t len;
};

/* Metadata for replaced instructions that we need to preserve until analysis time. */
static map<ADDRINT, vector<fake_inst_info_t>> replacements;

KNOB<string> KnobIgnoreFunctions(KNOB_MODE_WRITEONCE,
                                 "pintool",
                                 "ignore_functions",
                                 "",
                                 "Comma-separated list of functions to replace with a nop");

extern KNOB<BOOL> KnobIgnoringInstructions;

/* Encode replacement instructions in the provided buffer. */
static size_t Encode(xed_encoder_instruction_t inst, uint8_t* inst_bytes) {
    xed_encoder_request_t enc_req;

    xed_encoder_request_zero_set_mode(&enc_req, &dstate);

    bool convert_ok = xed_convert_to_encoder_request(&enc_req, &inst);
    if (!convert_ok) {
        cerr << "conversion to encode request failed" << endl;
        PIN_ExitProcess(EXIT_FAILURE);
    }

    size_t inst_len;
    auto err = xed_encode(&enc_req, inst_bytes, xiosim::x86::MAX_ILEN, &inst_len);
    if (err != XED_ERROR_NONE) {
        cerr << "xed_encode failed " << xed_error_enum_t2str(err) << endl;
        PIN_ExitProcess(EXIT_FAILURE);
    }
    return inst_len;
}

/* Start ignoring instruction before the replaced call, and add the magically-encoded
 * replacement instructions to the producer buffers. */
static void ReplacedFunctionBefore(THREADID tid, ADDRINT pc, ADDRINT retPC) {
    thread_state_t* tstate = get_tls(tid);

    tstate->lock.lock();
    tstate->ignore = true;
    tstate->lock.unlock();

    auto repl_state = replacements[pc];

    for (auto& inst : repl_state) {
        bool last_inst = (inst == repl_state.back());
        handshake_container_t* handshake = xiosim::buffer_management::GetBuffer(tstate->tid);

        handshake->asid = asid;
        handshake->flags.valid = true;
        handshake->flags.real = false;
        handshake->flags.isFirstInsn = false;
        handshake->flags.speculative = speculation_mode;

        handshake->pc = inst.pc;
        handshake->tpc = inst.pc + inst.len;
        handshake->npc = last_inst ? NextUnignoredPC(retPC) : inst.pc + inst.len;
        handshake->flags.brtaken = false;
        memcpy(handshake->ins, (void*)inst.pc, inst.len);

#ifdef PRINT_DYN_TRACE
        printTrace("sim", handshake->pc, tid);
#endif

        xiosim::buffer_management::ProducerDone(tstate->tid);
    }
}

/* Stop ignoring instruction after the replaced call. */
static void ReplacedFunctionAfter(THREADID tid) {
    thread_state_t* tstate = get_tls(tid);

    tstate->lock.lock();
    tstate->ignore = false;
    tstate->lock.unlock();
}

/* Helper struct to send parameters to image instrumentation routines. */
struct replacement_params_t {
    string function_name;
    size_t num_params;
    list<xed_encoder_instruction_t> insts;
};

static void AddReplacementCalls(IMG img, void* v) {
    replacement_params_t* params = static_cast<replacement_params_t*>(v);

    RTN rtn = RTN_FindByName(img, params->function_name.c_str());
    if (RTN_Valid(rtn)) {
        ADDRINT rtn_pc = RTN_Address(rtn);

        /* Don't allow multiple replacements of the same functions. */
        if (replacements.find(rtn_pc) != replacements.end()) {
            cerr << "Routine " << params->function_name << " already replaced." << endl;
            PIN_ExitProcess(EXIT_FAILURE);
        }

        /* Allocate space for the replacement instructions so they have real PCs. */
        void* inst_buffer = malloc(params->insts.size() * xiosim::x86::MAX_ILEN);
        if (inst_buffer == nullptr) {
            cerr << "Failed to alloc inst buffer. " << endl;
            PIN_ExitProcess(EXIT_FAILURE);
        }

        /* Encode replacement instructions and place them in inst_buffer. */
        vector<fake_inst_info_t> encoded_insts;
        uint8_t* curr_buffer = static_cast<uint8_t*>(inst_buffer);
        size_t bytes_used = 0;
        for (auto& x : params->insts) {
            size_t n_bytes = Encode(x, curr_buffer);
            encoded_insts.push_back(fake_inst_info_t((ADDRINT)curr_buffer, n_bytes));
            curr_buffer += n_bytes;
            bytes_used += n_bytes;
        }

        /* Add the actual instrumentation callbacks. */
        RTN_Open(rtn);
        RTN_InsertCall(rtn,
                       IPOINT_BEFORE,
                       AFUNPTR(ReplacedFunctionBefore),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_RETURN_IP,
                       IARG_CALL_ORDER,
                       CALL_ORDER_FIRST,
                       IARG_END);
        RTN_InsertCall(rtn,
                       IPOINT_AFTER,
                       AFUNPTR(ReplacedFunctionAfter),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER,
                       CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);

        /* Make sure ignoring API is enabled, otherwise below does nothing. */
        ASSERTX(KnobIgnoringInstructions.Value());
        /* Fixup next PC in instrumentation. */
        IgnoreCallsTo(
            rtn_pc, params->num_params + 1 /* the call + param pushes */, (ADDRINT)inst_buffer);

        replacements[rtn_pc] = encoded_insts;
    }
}

void
AddReplacement(string function_name, size_t num_params, list<xed_encoder_instruction_t> insts) {
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
    AddReplacement(function_name, 0, insts);
}
