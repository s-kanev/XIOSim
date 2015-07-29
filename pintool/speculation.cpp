#include <sys/wait.h>
#include <iostream>
#ifdef SPECULATION_DEBUG
#include <chrono>
#endif

#include "buffer.h"
#include "BufferManagerProducer.h"
#include "feeder.h"
#include "speculation.h"

#include "../decode.h"

/* Chicken bit to disable speculation. */
KNOB<BOOL> KnobSpeculation(KNOB_MODE_WRITEONCE,
                           "pintool",
                           "speculation",
                           "true",
                           "Fork feeder processes on speculative paths");

bool speculation_mode = false;

static const int max_speculated_inst = 100;

#ifdef SPECULATION_DEBUG
using namespace std::chrono;
static time_point<system_clock> child_start_time, child_end_time;
#endif

/* Invoke this thread's branch predictor. Ideally, its predictions
 * should match the ones done in HW, but we don't rely on that for
 * correctness, only for precision.
 * Return value:
 *   Pair of (correct prediction, predicted NPC).
 */
static pair<bool, md_addr_t>
BranchPredict(thread_state_t* tstate, handshake_container_t* handshake) {
    /* Proxy for Mop->decode.is_ctrl */
    if (handshake->npc == handshake->tpc)
        return std::make_pair(true, 0);

    class bpred_t* bpred = tstate->bpred;

    /* Decode a fake instruction so we can get flags that the branch predictor
     * requires. This is costly (slow & we'll do it again in the oracle).
     * Have to figure out something smarter. To begin with, the bpred really
     * shouldn't use decode information. TODO. */
    struct Mop_t jnk_Mop;
    memcpy(jnk_Mop.fetch.inst.code, handshake->ins, xiosim::x86::MAX_ILEN);
    xiosim::x86::decode(&jnk_Mop);
    xiosim::x86::decode_flags(&jnk_Mop);

    class bpred_state_cache_t* bpred_update = bpred->get_state_cache();
    md_addr_t oracle_NPC = handshake->flags.brtaken ? handshake->tpc : handshake->npc;
    /* Make a prediction. */
    md_addr_t pred_NPC = bpred->lookup(bpred_update,
                                       jnk_Mop.decode.opflags,
                                       handshake->pc,
                                       handshake->npc,
                                       handshake->tpc,
                                       oracle_NPC,
                                       handshake->flags.brtaken);
    /* Speculatively update predictor tables, mostly relevant for RAS. */
    bpred->spec_update(bpred_update,
                       jnk_Mop.decode.opflags,
                       handshake->pc,
                       handshake->tpc,
                       oracle_NPC,
                       bpred_update->our_pred);

    bool correct = (pred_NPC == oracle_NPC);
    /* Mispredicted, some tables (mostly RAS) will be recovered during the flush. */
    if (!correct)
        bpred->recover(bpred_update, handshake->flags.brtaken);

    /* Update tables (mostly BTBs) when commiting the branch. */
    bpred->update(bpred_update,
                  jnk_Mop.decode.opflags,
                  handshake->pc,
                  handshake->npc,
                  handshake->tpc,
                  oracle_NPC,
                  handshake->flags.brtaken);
    bpred->return_state_cache(bpred_update);

    return std::make_pair(correct, pred_NPC);
}

/* Return non-zero for the next analysis routine (Speculation) to be called. */
static ADDRINT CheckForSpeculation(THREADID tid) {
    thread_state_t* tstate = get_tls(tid);

    /* We're speculating and have reached the speculation limit. We're done. */
    if (speculation_mode && tstate->num_inst >= max_speculated_inst)
        FinishSpeculation(tstate);

    /* We're speculating. Let's not deal with nested speculation. */
    if (speculation_mode)
        return 0;

    /* In some corner cases (still ignroing instructions) there is no
     * instruction to speculate afer. */
    if (xiosim::buffer_management::ProducerEmpty(tstate->tid))
        return 0;

    /* Previous speculation has finished generating a handshake with a correct
     * NPC value. We use it as ground truth. */
    handshake_container_t* handshake = xiosim::buffer_management::Back(tstate->tid);
    assert(handshake->flags.valid);

    auto prediction = BranchPredict(tstate, handshake);
    tstate->lastBranchPrediction = prediction.second;
    return (prediction.first == false);
}

/* Fork a child feeder that will produce instructions on the speculative path.
 * It's ok if that feeder does something bad (say segfaults) -- in that case,
 * we'll just pick up from the non-speculative path.
 * The parent feeder waits until the child is done -- this way we can still
 * maintain a linear flow of instructions through the handshake buffers.
 * XXX: We need to run pin with "-catch_signals 0". Otherwise the speculative
 * feeder gets hung up on some corner cases like (jmp DWORD PTR [0x0]).
 */
static void Speculate(THREADID tid, CONTEXT* ctxt) {
    thread_state_t* tstate = get_tls(tid);

    /* Flush parent producer buffer before forking. */
    xiosim::buffer_management::FlushBuffers(tstate->tid);

#ifdef SPECULATION_DEBUG
    time_point<system_clock> start_time, end_time;
    start_time = system_clock::now();
#endif

    pid_t child = fork();
    switch (child) {
    case 0: {  // child
#ifdef SPECULATION_DEBUG
        child_start_time = system_clock::now();
#endif
        /* We're officially speculating now. */
        speculation_mode = true;
        /* Zero instructions produced. We'll use this to limit speculation length. */
        tstate->num_inst = 0;

        /* Send the simulated app along the latest predicted path.
         * ExecuteAt doesn't return -- we rely that this is the last analysis routine. */
        PIN_SetContextRegval(ctxt,
                             LEVEL_BASE::REG_INST_PTR,
                             reinterpret_cast<const UINT8*>(&tstate->lastBranchPrediction));
        PIN_ExecuteAt(ctxt);
        break;
    }
    case 1: {
        std::cerr << "Fork failed." << std::endl;
        exit(EXIT_FAILURE);
        break;
    }
    default: {  // parent
        /* Wait until child terminates (cleanly or not).
         * XXX: I've tried about 5 different versions of this and it seems that
         * just waitpid-ing is the most reliable (and among the fastests).
         * It costs ~1ms, when the speculation itself can easily go in the 10-s of ms.
         * Still, we need to optimize more if we want any reasonable (>100 KIPS) perf.
         */
        int status;
        pid_t wait_res;
        do {
            wait_res = waitpid(child, &status, 0);
            if (wait_res == -1) {
                std::cerr << "waitpid(" << child << ") failed." << std::endl;
                break;
            }
        } while (wait_res != child);
#ifdef SPECULATION_DEBUG
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            std::cerr << "Child died." << std::endl;
        }
#endif

#ifdef SPECULATION_DEBUG
        end_time = system_clock::now();
        std::cerr << "Parent: " << duration_cast<milliseconds>(end_time - start_time).count()
                  << " ms" << std::endl;
#endif

        break;
    }
    }
}

/* This speculative process is done producing handshakes.
 * Flush anything it has created to the shared memory file buffer, and
 * terminate the feeder process. The parent, non-speculative feeder will
 * pick up and continue producing on the non-spec path.
 */
void FinishSpeculation(thread_state_t* tstate) {
    assert(speculation_mode);
    handshake_container_t* last = xiosim::buffer_management::Back(tstate->tid);
    assert(last->flags.valid);

#ifdef SPECULATION_DEBUG
    std::cerr << "Speculated " << tstate->num_inst << " instructions. Terminating cleanly."
              << std::endl;
#endif

    /* Flush these hard-earned instructions. */
    xiosim::buffer_management::FlushBuffers(tstate->tid);

#ifdef SPECULATION_DEBUG
    child_end_time = system_clock::now();
    std::cerr << "Child: " << duration_cast<milliseconds>(child_end_time - child_start_time).count()
              << " ms" << std::endl;
#endif

    PIN_ExitProcess(EXIT_SUCCESS);
    return;
}

VOID InstrumentSpeculation(INS ins, VOID* v) {
    if (!KnobSpeculation.Value())
        return;

    INS_InsertIfCall(ins,
                     IPOINT_BEFORE,
                     (AFUNPTR)CheckForSpeculation,
                     IARG_THREAD_ID,
                     IARG_CALL_ORDER,
                     CALL_ORDER_LAST,
                     IARG_END);
    INS_InsertThenCall(ins,
                       IPOINT_BEFORE,
                       (AFUNPTR)Speculate,
                       IARG_THREAD_ID,
                       IARG_CONTEXT,
                       IARG_CALL_ORDER,
                       CALL_ORDER_LAST,
                       IARG_END);
}
