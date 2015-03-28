#include "boost_interprocess.h"

#include "ezOptionParser_clean.hpp"

#include "../interface.h"
#include "multiprocess_shared.h"
#include "ipc_queues.h"

#include "scheduler.h"
#include "../synchronization.h"
#include "../buffer.h"
#include "BufferManagerConsumer.h"
#include "../zesto-core.h"
#include "allocators_impl.h"

#include "timing_sim.h"

const char sim_name[] = "XIOSim";

extern int num_cores;

static sim_thread_state_t thread_states[MAX_CORES];

inline sim_thread_state_t* get_sim_tls(int coreID)
{
    return &thread_states[coreID];
}

using namespace std;
using namespace xiosim; //Until we namespace everything

BaseAllocator *core_allocator = NULL;

/* ==========================================================================
 * Called from simulator once handshake is free to be reused.
 * This allows to pipeline instrumentation and simulation. */
void ReleaseHandshake(int coreID)
{
    pid_t instrument_tid = GetCoreThread(coreID);
    assert(!xiosim::buffer_management::empty(instrument_tid));

    // pop() invalidates the buffer
    xiosim::buffer_management::pop(instrument_tid);
}

/* ========================================================================== */
/* The loop running each simulated core. */
void* SimulatorLoop(void* arg)
{
    int coreID = reinterpret_cast<int>(arg);
    sim_thread_state_t *tstate = get_sim_tls(coreID);

    while (true) {
        /* Check kill flag */
        lk_lock(&tstate->lock, 1);

        if (!tstate->is_running) {
            deactivate_core(coreID);
            tstate->sim_stopped = true;
            lk_unlock(&tstate->lock);
            return NULL;
        }
        lk_unlock(&tstate->lock);

        /* Check for messages coming from producer processes
         * and execute accordingly */
        CheckIPCMessageQueue(true, coreID);

        // Get the latest thread we are running from the scheduler
        pid_t instrument_tid = GetCoreThread(coreID);
        if (instrument_tid == INVALID_THREADID) {
            xio_sleep(10);
            continue;
        }

        while (xiosim::buffer_management::empty(instrument_tid)) {
            yield();
            wait_consumers();
        }

        int consumerHandshakes = xiosim::buffer_management::getConsumerSize(instrument_tid);
        if(consumerHandshakes == 0) {
            xiosim::buffer_management::front(instrument_tid, false);
            consumerHandshakes = xiosim::buffer_management::getConsumerSize(instrument_tid);
        }
        assert(consumerHandshakes > 0);

        int numConsumed = 0;
        for(int i = 0; i < consumerHandshakes; i++) {
            wait_consumers();

            handshake_container_t* handshake = xiosim::buffer_management::front(instrument_tid, true);
            assert(handshake != NULL);
            assert(handshake->flags.valid);

            // Check thread exit flag
            if (handshake->flags.killThread) {
                ReleaseHandshake(coreID);
                // Apply buffer changes before notifying the scheduler,
                // doing otherwise would result in a race
                numConsumed++;
                xiosim::buffer_management::applyConsumerChanges(instrument_tid, numConsumed);
                numConsumed = 0;

                // Let the scheduler send something else to this core
                DescheduleActiveThread(coreID);

                assert(i == consumerHandshakes-1); // Check that there are no more handshakes
                break;
            }

            if (handshake->flags.giveCoreUp) {
                ReleaseHandshake(coreID);

                // Apply buffer changes before notifying the scheduler,
                // doing otherwise would result in a race
                numConsumed++;
                xiosim::buffer_management::applyConsumerChanges(instrument_tid, numConsumed);
                numConsumed = 0;

                // Let the scheduler send something else to this core
                GiveUpCore(coreID, handshake->flags.giveUpReschedule);
                break;
            }

            // First instruction, map stack pages, and flag we're not safe to kill
            if (handshake->flags.isFirstInsn)
            {
                md_addr_t esp = handshake->handshake.ctxt.regs_R.dw[MD_REG_ESP];
                md_addr_t bos;
                lk_lock(lk_thread_bos, 1);
                bos = thread_bos->at(instrument_tid);
                lk_unlock(lk_thread_bos);
                Zesto_Map_Stack(handshake->handshake.asid, esp, bos);

                lk_lock(&tstate->lock, 1);
                tstate->sim_stopped = false;
                lk_unlock(&tstate->lock);
            }

            // Actual simulation happens here
            Zesto_Resume(coreID, handshake);

            numConsumed++;

            if (NeedsReschedule(coreID)) {
                // Apply buffer changes before notifying the scheduler,
                // doing otherwise would result in a race
                xiosim::buffer_management::applyConsumerChanges(instrument_tid, numConsumed);
                numConsumed = 0;

                GiveUpCore(coreID, true);
                break;
            }
        }
        xiosim::buffer_management::applyConsumerChanges(instrument_tid, numConsumed);
    }
    return NULL;
}

/* ========================================================================== */
/* Create simulator threads, and wait until they finish. */
void SpawnSimulatorThreads(int numCores)
{
    pthread_t * threads = new pthread_t[numCores];

    /* Spawn all threads */
    for (int i=0; i<numCores; i++) {
        pthread_t child;
        int res = pthread_create(&child, NULL, SimulatorLoop, reinterpret_cast<void*>(i));
        if (res != 0) {
            cerr << "Failed spawning sim thread " << i << endl;
            abort();
        }
        cerr << "Spawned sim thread " << i << endl;
        threads[i] = child;
    }

    /* and sleep until the simulation finishes */
    for (int i=0; i<numCores; i++) {
        pthread_join(threads[i], NULL);
    }
}

/* ========================================================================== */
/* Invariant: we are not simulating anything here. Either:
 * - Not in a pinpoints ROI.
 * - Anything after PauseSimulation.
 * This implies all cores are inactive. And handshake buffers are already drained. */
void StopSimulation(bool kill_sim_threads, int caller_coreID)
{
    if (kill_sim_threads) {
        /* Signal simulator threads to die */
        for (int coreID=0; coreID < num_cores; coreID++) {
            sim_thread_state_t* curr_tstate = get_sim_tls(coreID);
            lk_lock(&curr_tstate->lock, 1);
            curr_tstate->is_running = false;
            lk_unlock(&curr_tstate->lock);
        }

        /* Spin until SimulatorLoop actually finishes */
        volatile bool is_stopped;
        do {
            is_stopped = true;

            for (int coreID=0; coreID < num_cores; coreID++) {
                if (coreID == caller_coreID)
                    continue;

                sim_thread_state_t* curr_tstate = get_sim_tls(coreID);
                lk_lock(&curr_tstate->lock, 1);
                is_stopped &= curr_tstate->sim_stopped;
                lk_unlock(&curr_tstate->lock);
            }
        } while(!is_stopped);
    }

    Zesto_Destroy();

    if (kill_sim_threads)
        exit(EXIT_SUCCESS);
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
typedef pair <unsigned int, char **> SSARGS;
SSARGS MakeSimpleScalarArgcArgv(unsigned int argc, const char *argv[])
{
    char   **ssArgv   = 0;
    unsigned int ssArgBegin = 0;
    unsigned int ssArgc     = 0;
    unsigned int ssArgEnd   = argc;

    for (unsigned int i = 0; i < argc; i++)
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
                 + (2);                     // Null terminator for argv
    }
    else
    {
        // Coming here implies the command line has not been setup properly even
        // to run Pin, so return. Pin will complain appropriately.
        return make_pair(argc, const_cast <char **>(argv));
    }

    // This buffer will hold SimpleScalar's argv
    ssArgv = reinterpret_cast <char **> (calloc(ssArgc, sizeof(char *)));
    if (ssArgv == NULL) {
        std::cerr << "Calloc argv failed" << std::endl;
        abort();
    }

    unsigned int ssArgIndex = 0;
    ssArgv[ssArgIndex++] = const_cast <char*>(sim_name);  // Does not matter; just for sanity
    for (unsigned int pin = ssArgBegin; pin < ssArgEnd; pin++)
    {
        if (string(argv[pin]) != "--")
        {
            string *argvCopy = new string(argv[pin]);
            ssArgv[ssArgIndex++] = const_cast <char *> (argvCopy->c_str());
        }
    }

    // Terminate the argv. Ending must terminate with a pointer *referencing* a
    // NULL. Simply terminating the end of argv[n] w/ NULL violates conventions
    ssArgv[ssArgIndex++] = new char('\0');

    return make_pair(ssArgc, ssArgv);
}

/* ========================================================================== */
int main(int argc, const char * argv[])
{
    ez::ezOptionParser opts;
    opts.overview = "XIOSim timing_sim options";
    opts.syntax = "XXX";
    opts.add("-1", 1, 1, 0, "Harness PID", "-harness_pid");
    opts.add("1", 1, 1, 0, "Number of cores simulated", "-num_cores");
    opts.parse(argc, argv);

    int harness_pid;
    opts.get("-harness_pid")->getInt(harness_pid);
    int num_cores;
    opts.get("-num_cores")->getInt(num_cores);

    InitSharedState(false, harness_pid, num_cores);
    xiosim::buffer_management::InitBufferManagerConsumer(harness_pid, num_cores);

    // Prepare args for libsim
    SSARGS ssargs = MakeSimpleScalarArgcArgv(argc, argv);
    Zesto_SlaveInit(ssargs.first, ssargs.second);

    InitScheduler(num_cores);
    // The following core/uncore power values correspond to 20% of total system
    // power going to the uncore.
    core_allocator = &(AllocatorParser::Get(
                knobs.allocator,
                knobs.allocator_opt_target,
                knobs.speedup_model,
                1,  // core_power
                num_cores / (1/0.2 - 1),  // uncore_power
                num_cores));
    SpawnSimulatorThreads(num_cores);

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

        std::vector<int> ack_list;
        bool ack_list_valid = false;

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
            case SCHEDULE_PROCESS_THREADS:
                {
                    list<pid_t> threads;
                    for (unsigned int i=0; i < ipcMessage.MAX_ARR_SIZE; i++) {
                        int32_t tid = (int32_t)ipcMessage.arg_array[i];
                        if (tid != -1)
                            threads.push_back(tid);
                    }
                    xiosim::ScheduleProcessThreads(ipcMessage.arg0, threads);
                }
                break;
            case ALLOCATE_THREAD:
                xiosim::buffer_management::AllocateThreadConsumer(ipcMessage.arg0, ipcMessage.arg1);
                break;
            case THREAD_AFFINITY:
                xiosim::SetThreadAffinity(ipcMessage.arg0, ipcMessage.arg1);
                break;
            case ALLOCATE_CORES:
                {
                    vector<double> scaling;
                    for (unsigned int i=0; i < ipcMessage.MAX_ARR_SIZE; i++) {
                        double speedup = ipcMessage.arg_array[i];
                        if (speedup != -1)
                            scaling.push_back(speedup);
                    }
                    core_allocator->AllocateCoresForProcess(ipcMessage.arg0,
                        scaling, ipcMessage.arg1);
                    ack_list = core_allocator->get_processes_to_unblock(ipcMessage.arg0);
                    ack_list_valid = true;
                    break;
                }
            case DEALLOCATE_CORES:
                core_allocator->DeallocateCoresForProcess(ipcMessage.arg0);
                break;
            default:
                abort();
                break;
        }

        /* Handle blocking message acknowledgement. */
        if (ipcMessage.blocking) {
            /* Typically, a blocking message is ack-ed straight away and
             * all is good with the world. */
            if (!ack_list_valid) {
                lk_lock(lk_ipcMessageQueue, 1);
                assert(ackMessages->at(ipcMessage) == false);
                ackMessages->at(ipcMessage) = true;
                lk_unlock(lk_ipcMessageQueue);
            }
            else {
            /* Some messages are special. They want to ack a (possibly empty)
             * list of messages of the same type (say, ack everyone after we are
             * done with the last one). */
                lk_lock(lk_ipcMessageQueue, 1);
                for (int unblock_asid : ack_list) {
                    for (auto ack_it = ackMessages->begin();
                         ack_it != ackMessages->end(); ++ack_it) {
                        if (ack_it->first.id == ipcMessage.id &&
                            ack_it->first.arg0 == unblock_asid) {
                            assert(ack_it->second == false);
                            ackMessages->at(ack_it->first) = true;
                        }
                    }
                }
                lk_unlock(lk_ipcMessageQueue);
            }
        }
    }
}
