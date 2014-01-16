#include "pin.H"
#include "instlib.H"
using namespace INSTLIB;

#include "boost_interprocess.h"

#include "../interface.h"
#include "multiprocess_shared.h"
#include "ipc_queues.h"

#include "scheduler.h"
#include "../synchronization.h"
#include "../buffer.h"
#include "BufferManagerConsumer.h"
#include "../zesto-core.h"

#include "timing_sim.h"

// IDs of simulaiton threads
static THREADID *sim_threadid;

const char sim_name[] = "XIOSim";

extern int num_cores;

// Used to access thread-local storage
static TLS_KEY tls_key;

// Functions to access thread-specific data
/* ========================================================================== */
sim_thread_state_t* get_sim_tls(THREADID threadid)
{
    sim_thread_state_t* tstate =
          static_cast<sim_thread_state_t*>(PIN_GetThreadData(tls_key, threadid));
    return tstate;
}

/* ==========================================================================
 * Called from simulator once handshake is free to be reused.
 * This allows to pipeline instrumentation and simulation. */
VOID ReleaseHandshake(UINT32 coreID)
{
    pid_t instrument_tid = GetCoreThread(coreID);
    ASSERTX(!xiosim::buffer_management::empty(instrument_tid));

    // pop() invalidates the buffer
    xiosim::buffer_management::pop(instrument_tid);
}

/* ========================================================================== */
/* The loop running each simulated core. */
VOID SimulatorLoop(VOID* arg)
{
    INT32 coreID = reinterpret_cast<INT32>(arg);
    THREADID sim_tid = PIN_ThreadId();

    sim_thread_state_t* tstate = new sim_thread_state_t();
    PIN_SetThreadData(tls_key, tstate, sim_tid);

    while (true) {
        /* Check kill flag */
        lk_lock(&tstate->lock, sim_tid+1);

        if (!tstate->is_running) {
            deactivate_core(coreID);
            tstate->sim_stopped = true;
            lk_unlock(&tstate->lock);
            return;
        }
        lk_unlock(&tstate->lock);

        /* Check for messages coming from producer processes
         * and execute accordingly */
        CheckIPCMessageQueue(true, coreID);

        if (!is_core_active(coreID)) {
            PIN_Sleep(10);
            continue;
        }

        // Get the latest thread we are running from the scheduler
        pid_t instrument_tid = GetCoreThread(coreID);
        if (instrument_tid == (pid_t)INVALID_THREADID) {
            continue;
        }

        while (xiosim::buffer_management::empty(instrument_tid)) {
            PIN_Yield();
            while(*consumers_sleep) {
                PIN_SemaphoreWait(consumer_sleep_lock);
            }
        }

        int consumerHandshakes = xiosim::buffer_management::getConsumerSize(instrument_tid);
        if(consumerHandshakes == 0) {
            xiosim::buffer_management::front(instrument_tid, false);
            consumerHandshakes = xiosim::buffer_management::getConsumerSize(instrument_tid);
        }
        assert(consumerHandshakes > 0);

        int numConsumed = 0;
        for(int i = 0; i < consumerHandshakes; i++) {
            while(*consumers_sleep) {
                PIN_SemaphoreWait(consumer_sleep_lock);
            }

            handshake_container_t* handshake = xiosim::buffer_management::front(instrument_tid, true);
            ASSERTX(handshake != NULL);
            ASSERTX(handshake->flags.valid);

            // Check thread exit flag
            if (handshake->flags.killThread) {
                ReleaseHandshake(coreID);
                numConsumed++;
                // Let the scheduler send something else to this core
                DescheduleActiveThread(coreID);

                ASSERTX(i == consumerHandshakes-1); // Check that there are no more handshakes
                break;
            }

            if (handshake->flags.giveCoreUp) {
                ReleaseHandshake(coreID);
                numConsumed++;
                GiveUpCore(coreID, handshake->flags.giveUpReschedule);
                break;
            }

            // First instruction, set bottom of stack, and flag we're not safe to kill
            if (handshake->flags.isFirstInsn)
            {
                Zesto_SetBOS(coreID, handshake->flags.BOS);
                lk_lock(&tstate->lock, sim_tid+1);
                tstate->sim_stopped = false;
                lk_unlock(&tstate->lock);
            }

            // Actual simulation happens here
            Zesto_Resume(coreID, handshake);

            numConsumed++;

            if (NeedsReschedule(coreID)) {
                GiveUpCore(coreID, true);
                break;
            }
        }
        xiosim::buffer_management::applyConsumerChanges(instrument_tid, numConsumed);
#if 0
        lastConsumerApply[instrument_tid] = cores[coreID]->sim_cycle;
#endif
    }
}

/* ========================================================================== */
/* Create simulator threads and set up their local storage */
VOID SpawnSimulatorThreads(INT32 numCores)
{
    cerr << numCores << endl;
    sim_threadid = new THREADID[numCores];

    THREADID tid;
    for(INT32 i=0; i<numCores; i++) {
        tid = PIN_SpawnInternalThread(SimulatorLoop, reinterpret_cast<VOID*>(i), 0, NULL);
        if (tid == INVALID_THREADID) {
            cerr << "Failed spawning sim thread " << i << endl;
            PIN_ExitProcess(EXIT_FAILURE);
        }
        sim_threadid[i] = tid;
        cerr << "Spawned sim thread " << i << " " << tid << endl;
    }
}

/* ========================================================================== */
/* Invariant: we are not simulating anything here. Either:
 * - Not in a pinpoints ROI.
 * - Anything after PauseSimulation (or the HELIX equivalent)
 * This implies all cores are inactive. And handshake buffers are already drained. */
VOID StopSimulation(BOOL kill_sim_threads, int caller_coreID)
{
    /* For now, force end the slice (once!), once all processes have finished. */
    Zesto_Slice_End(1, 0, 100 * 1000);

    if (kill_sim_threads) {
        /* Signal simulator threads to die */
        INT32 coreID;
        for (coreID=0; coreID < num_cores; coreID++) {
            sim_thread_state_t* curr_tstate = get_sim_tls(sim_threadid[coreID]);
            lk_lock(&curr_tstate->lock, 1);
            curr_tstate->is_running = false;
            lk_unlock(&curr_tstate->lock);
        }

        /* Spin until SimulatorLoop actually finishes */
        volatile bool is_stopped;
        do {
            is_stopped = true;

            for (coreID=0; coreID < num_cores; coreID++) {
                if (coreID == caller_coreID)
                    continue;

                sim_thread_state_t* curr_tstate = get_sim_tls(sim_threadid[coreID]);
                lk_lock(&curr_tstate->lock, 1);
                is_stopped &= curr_tstate->sim_stopped;
                lk_unlock(&curr_tstate->lock);
            }
        } while(!is_stopped);
    }

    Zesto_Destroy();

    if (kill_sim_threads)
        PIN_ExitProcess(EXIT_SUCCESS);
}

/* ========================================================================== */
/** The command line arguments passed upon invocation need paring because (1) the
 * command line can have arguments for SimpleScalar and (2) Pin cannot see the
 * SimpleScalar's arguments otherwise it will barf; it'll expect KNOB
 * declarations for those arguments. Thereforce, we follow a convention that
 * anything declared past "-s" and before "--" on the command line must be
 * passed along as SimpleScalar's argument list.
 *
 * SimpleScalar's arguments are extracted out of the command line in twos steps:
 * First, we create a new argument vector that can be passed to SimpleScalar.
 * This is done by calloc'ing and copying the arguments over. Thereafter, in the
 * second stage we nullify SimpleScalar's arguments from the original (Pin's)
 * command line so that Pin doesn't see during its own command line parsing
 * stage. */
typedef pair <UINT32, CHAR **> SSARGS;
SSARGS MakeSimpleScalarArgcArgv(UINT32 argc, CHAR *argv[])
{
    CHAR   **ssArgv   = 0;
    UINT32 ssArgBegin = 0;
    UINT32 ssArgc     = 0;
    UINT32 ssArgEnd   = argc;

    for (UINT32 i = 0; i < argc; i++)
    {
        if ((string(argv[i]) == "-s") || (string(argv[i]) == "--"))
        {
            ssArgBegin = i + 1;             // Points to a valid arg
            break;
        }
    }

    if (ssArgBegin)
    {
        ssArgc = (ssArgEnd - ssArgBegin)    // Specified command line args
                 + (1);                     // Null terminator for argv
    }
    else
    {
        // Coming here implies the command line has not been setup properly even
        // to run Pin, so return. Pin will complain appropriately.
        return make_pair(argc, argv);
    }

    // This buffer will hold SimpleScalar's argv
    ssArgv = reinterpret_cast <CHAR **> (calloc(ssArgc, sizeof(CHAR *)));

    UINT32 ssArgIndex = 0;
    ssArgv[ssArgIndex++] = const_cast <CHAR*>(sim_name);  // Does not matter; just for sanity
    for (UINT32 pin = ssArgBegin; pin < ssArgEnd; pin++)
    {
        if (string(argv[pin]) != "--")
        {
            string *argvCopy = new string(argv[pin]);
            ssArgv[ssArgIndex++] = const_cast <CHAR *> (argvCopy->c_str());
        }
    }

    // Terminate the argv. Ending must terminate with a pointer *referencing* a
    // NULL. Simply terminating the end of argv[n] w/ NULL violates conventions
    ssArgv[ssArgIndex++] = new CHAR('\0');

    return make_pair(ssArgc, ssArgv);
}

/* ========================================================================== */
int main(int argc, char * argv[])
{
    PIN_Init(argc, argv);
    PIN_InitSymbols();

    // Obtain  a key for TLS storage.
    tls_key = PIN_CreateThreadDataKey(0);

    InitSharedState(false, KnobHarnessPid.Value());
    xiosim::buffer_management::InitBufferManagerConsumer();

    // Prepare args for libsim
    SSARGS ssargs = MakeSimpleScalarArgcArgv(argc, argv);
    Zesto_SlaveInit(ssargs.first, ssargs.second);

    ASSERTX( num_cores == KnobNumCores.Value() );

    InitScheduler(num_cores);
    SpawnSimulatorThreads(num_cores);

    PIN_StartProgram();

    return 0;
}

void CheckIPCMessageQueue(bool isEarly, int caller_coreID)
{
    /* Grab a message from IPC queue in shared memory */
    while (true) {
        ipc_message_t ipcMessage;
        MessageQueue *q = isEarly ? ipcEarlyMessageQueue : ipcMessageQueue;

        lk_lock(lk_ipcMessageQueue, 1);
        if (q->empty()) {
            lk_unlock(lk_ipcMessageQueue);
            break;
        }

        ipcMessage = q->front();
        q->pop_front();
        lk_unlock(lk_ipcMessageQueue);

#ifdef IPC_DEBUG
        lk_lock(printing_lock, 1);
        std::cerr << "IPC message, ID: " << ipcMessage.id << " early: " << isEarly << std::endl;
        lk_unlock(printing_lock);
#endif

        /* And execute the appropriate call based on the protocol
         * defined in interface.h */
        switch(ipcMessage.id) {
            /* Sim control related */
            case SLICE_START:
                Zesto_Slice_Start(ipcMessage.arg0);
                break;
            case SLICE_END:
                Zesto_Slice_End(ipcMessage.arg0, ipcMessage.arg1, ipcMessage.arg2);
                break;
            /* Shadow page table related */
            case MMAP:
                Zesto_Notify_Mmap(ipcMessage.arg0, ipcMessage.arg1, ipcMessage.arg2, ipcMessage.arg3);
                break;
            case MUNMAP:
                Zesto_Notify_Munmap(ipcMessage.arg0, ipcMessage.arg1, ipcMessage.arg2, ipcMessage.arg3);
                break;
            case UPDATE_BRK:
                Zesto_UpdateBrk(ipcMessage.arg0, ipcMessage.arg1, ipcMessage.arg2);
                break;
            /* Warm caches */
            case WARM_LLC:
                Zesto_WarmLLC(ipcMessage.arg0, ipcMessage.arg1, ipcMessage.arg2);
                break;
            case STOP_SIMULATION:
                StopSimulation(ipcMessage.arg0, caller_coreID);
                break;
            case ACTIVATE_CORE: 
                activate_core(ipcMessage.arg0);
                break;
            case DEACTIVATE_CORE: 
                deactivate_core(ipcMessage.arg0);
                break;
            case SCHEDULE_NEW_THREAD:
                ScheduleNewThread(ipcMessage.arg0);
                break;
            case ALLOCATE_THREAD:
                xiosim::buffer_management::AllocateThreadConsumer(ipcMessage.arg0, ipcMessage.arg1);
                break;
            default:
                abort();
                break;
        }
    }
}
