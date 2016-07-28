#include "ezOptionParser_clean.hpp"

#include "xiosim/core_const.h"
#include "xiosim/knobs.h"
#include "xiosim/libsim.h"
#include "xiosim/memory.h"
#include "xiosim/sim.h"
#include "xiosim/slices.h"
#include "xiosim/synchronization.h"
#include "xiosim/zesto-core.h"
#include "xiosim/zesto-exec.h"
#include "xiosim/zesto-config.h"

#include "BufferManagerConsumer.h"
#include "allocators_impl.h"
#include "ipc_queues.h"
#include "multiprocess_shared.h"
#include "scheduler.h"

#include "timing_sim.h"

/* configuration parameters/knobs */
struct core_knobs_t core_knobs;
struct uncore_knobs_t uncore_knobs;
struct system_knobs_t system_knobs;

static sim_thread_state_t thread_states[xiosim::MAX_CORES];

inline sim_thread_state_t* get_sim_tls(int coreID) { return &thread_states[coreID]; }

using namespace std;
using namespace xiosim;  // Until we namespace everything

BaseAllocator* core_allocator = NULL;

/* ========================================================================== */
/* The loop running each simulated core. */
void* SimulatorLoop(void* arg) {
    int coreID = static_cast<int>(reinterpret_cast<long>(arg));
    sim_thread_state_t* tstate = get_sim_tls(coreID);

    while (true) {
        /* Check kill flag */
        lk_lock(&tstate->lock, 1);

        if (!tstate->is_running) {
            xiosim::libsim::deactivate_core(coreID);
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

        /* Process all handshakes in the in-memory consumeBuffer_ at once.
         * If there are none, the first call to fron will populate the buffer.
         * Doing this helps reduce contention on the IPCMessageQueue lock, which we
         * don't need to bang on too frequently. */
        int consumerHandshakes = xiosim::buffer_management::GetConsumerSize(instrument_tid);
        if (consumerHandshakes == 0) {
            xiosim::buffer_management::Front(instrument_tid);
            consumerHandshakes = xiosim::buffer_management::GetConsumerSize(instrument_tid);
        }
        assert(consumerHandshakes > 0);

        for (int i = 0; i < consumerHandshakes; i++) {
            handshake_container_t* handshake = xiosim::buffer_management::Front(instrument_tid);
            assert(handshake != NULL);
            assert(handshake->flags.valid);

            // Check thread exit flag
            if (handshake->flags.killThread) {
                // invalidate the handshake
                xiosim::buffer_management::Pop(instrument_tid);

                // Let the scheduler send something else to this core
                DescheduleActiveThread(coreID);
                break;
            }

            if (handshake->flags.giveCoreUp) {
                bool should_reschedule = handshake->flags.giveUpReschedule;

                // invalidate the handshake
                xiosim::buffer_management::Pop(instrument_tid);

                // Let the scheduler send something else to this core
                cores[coreID]->exec->flush_size_class_cache();
                GiveUpCore(coreID, should_reschedule);
                break;
            }

            if (handshake->flags.blockThread) {
                pid_t blocked_on = handshake->mem_buffer.front().first;

                // invalidate the handshake
                xiosim::buffer_management::Pop(instrument_tid);

                BlockThread(coreID, instrument_tid, blocked_on);
                break;
            }

            if (handshake->flags.setThreadAffinity) {
                int affine_coreID = handshake->mem_buffer.front().first;

                // invalidate the handshake
                xiosim::buffer_management::Pop(instrument_tid);

                xiosim::SetThreadAffinity(instrument_tid, affine_coreID);
                xiosim::MigrateThread(instrument_tid, coreID);
                break;
            }

            // First instruction, map stack pages, and flag we're not safe to kill
            if (handshake->flags.isFirstInsn) {
                md_addr_t esp = handshake->rSP;
                md_addr_t bos;
                lk_lock(lk_thread_bos, 1);
                bos = thread_bos->at(instrument_tid);
                lk_unlock(lk_thread_bos);
                xiosim::memory::map_stack(handshake->asid, esp, bos);

                lk_lock(&tstate->lock, 1);
                tstate->sim_stopped = false;
                lk_unlock(&tstate->lock);
            }

            // Actual simulation happens here
            xiosim::libsim::simulate_handshake(coreID, handshake);

            // invalidate the handshake
            xiosim::buffer_management::Pop(instrument_tid);

            // The scheduler has decided it's our time to let go of this core.
            if (NeedsReschedule(coreID)) {
                GiveUpCore(coreID, true);
                break;
            }
        }
    }
    return NULL;
}

/* ========================================================================== */
/* Create simulator threads, and wait until they finish. */
void SpawnSimulatorThreads(int numCores) {
    pthread_t* threads = new pthread_t[numCores];

    /* Spawn all threads */
    for (int i = 0; i < numCores; i++) {
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
    for (int i = 0; i < numCores; i++) {
        pthread_join(threads[i], NULL);
    }

    delete[] threads;
}

/* ========================================================================== */
/* Invariant: we are not simulating anything here. Either:
 * - Not in a pinpoints ROI.
 * - Anything after PauseSimulation.
 * This implies all cores are inactive. And handshake buffers are already
 * drained. */
void StopSimulation(bool kill_sim_threads, int caller_coreID) {
    if (kill_sim_threads) {
        /* Signal simulator threads to die */
        for (int coreID = 0; coreID < system_knobs.num_cores; coreID++) {
            sim_thread_state_t* curr_tstate = get_sim_tls(coreID);
            lk_lock(&curr_tstate->lock, 1);
            curr_tstate->is_running = false;
            lk_unlock(&curr_tstate->lock);
        }

        /* Spin until SimulatorLoop actually finishes */
        volatile bool is_stopped;
        do {
            is_stopped = true;

            for (int coreID = 0; coreID < system_knobs.num_cores; coreID++) {
                if (coreID == caller_coreID)
                    continue;

                sim_thread_state_t* curr_tstate = get_sim_tls(coreID);
                lk_lock(&curr_tstate->lock, 1);
                is_stopped &= curr_tstate->sim_stopped;
                lk_unlock(&curr_tstate->lock);
            }
        } while (!is_stopped);
    }

    xiosim::buffer_management::DeinitBufferManagerConsumer();
    xiosim::libsim::deinit();
    // Free memory allocated by libconfuse for the configuration options.
    free_config();
    DeinitSharedState();

    if (kill_sim_threads)
        pthread_exit(NULL);
}

/* ========================================================================== */
int main(int argc, const char* argv[]) {
    ez::ezOptionParser opts;
    opts.overview = "XIOSim timing_sim options";
    opts.syntax = "XXX";
    opts.add("-1", 1, 1, 0, "Harness PID", "-harness_pid");
    opts.add("", 1, 1, 0, "Simulator config file", "-config");
    opts.parse(argc, argv);

    int harness_pid;
    opts.get("-harness_pid")->getInt(harness_pid);
    std::string cfg_file;
    opts.get("-config")->getString(cfg_file);

    /* Parse configuration file. This will populate all knobs. */
    read_config_file(cfg_file, &core_knobs, &uncore_knobs, &system_knobs);

    InitSharedState(false, harness_pid, system_knobs.num_cores);
    xiosim::buffer_management::InitBufferManagerConsumer(harness_pid);

    xiosim::libsim::init();
    print_config(stderr);
    fprintf(stderr, "\n");

    InitScheduler(system_knobs.num_cores);
    // The following core/uncore power values correspond to 20% of total system
    // power going to the uncore.
    core_allocator = &(AllocatorParser::Get(system_knobs.allocator,
                                            system_knobs.allocator_opt_target,
                                            system_knobs.speedup_model,
                                            1,                                       // core_power
                                            system_knobs.num_cores / (1 / 0.2 - 1),  // uncore_power
                                            system_knobs.num_cores));
    SpawnSimulatorThreads(system_knobs.num_cores);

    return 0;
}

void CheckIPCMessageQueue(bool isEarly, int caller_coreID) {
    /* Grab a message from IPC queue in shared memory */
    while (true) {
        ipc_message_t ipcMessage;
        MessageQueue* q = isEarly ? ipcEarlyMessageQueue : ipcMessageQueue;

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
        switch (ipcMessage.id) {
        /* Sim control related */
        case SLICE_START:
            start_slice(ipcMessage.arg0);
            break;
        case SLICE_END:
            end_slice(ipcMessage.arg0, ipcMessage.arg1, ipcMessage.arg2);
            break;
        /* Shadow page table related */
        case MMAP:
            xiosim::memory::notify_mmap(
                    ipcMessage.arg0, ipcMessage.arg1, ipcMessage.arg2, ipcMessage.arg3);
            break;
        case MUNMAP:
            xiosim::memory::notify_munmap(
                    ipcMessage.arg0, ipcMessage.arg1, ipcMessage.arg2, ipcMessage.arg3);
            break;
        case UPDATE_BRK:
            xiosim::memory::update_brk(ipcMessage.arg0, ipcMessage.arg1, ipcMessage.arg2);
            break;
        /* Warm caches */
        case WARM_LLC:
            xiosim::libsim::simulate_warmup(ipcMessage.arg0, ipcMessage.arg1, ipcMessage.arg2);
            break;
        case STOP_SIMULATION:
            StopSimulation(ipcMessage.arg0, caller_coreID);
            break;
        case ACTIVATE_CORE:
            xiosim::libsim::activate_core(ipcMessage.arg0);
            break;
        case DEACTIVATE_CORE:
            xiosim::libsim::deactivate_core(ipcMessage.arg0);
            break;
        case SCHEDULE_NEW_THREAD:
            ScheduleNewThread(ipcMessage.arg0);
            break;
        case SCHEDULE_PROCESS_THREADS: {
            list<pid_t> threads;
            for (unsigned int i = 0; i < ipcMessage.MAX_ARR_SIZE; i++) {
                int32_t tid = (int32_t)ipcMessage.arg_array[i];
                if (tid != -1)
                    threads.push_back(tid);
            }
            xiosim::ScheduleProcessThreads(ipcMessage.arg0, threads);
        } break;
        case ALLOCATE_THREAD:
            xiosim::buffer_management::AllocateThreadConsumer(ipcMessage.arg0, ipcMessage.arg1);
            break;
        case THREAD_AFFINITY:
            xiosim::SetThreadAffinity(ipcMessage.arg0, ipcMessage.arg1);
            break;
        case ALLOCATE_CORES: {
            vector<double> scaling;
            for (unsigned int i = 0; i < ipcMessage.MAX_ARR_SIZE; i++) {
                double speedup = ipcMessage.arg_array[i];
                if (speedup != -1)
                    scaling.push_back(speedup);
            }
            core_allocator->AllocateCoresForProcess(ipcMessage.arg0, scaling, ipcMessage.arg1);
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
            } else {
                /* Some messages are special. They want to ack a (possibly empty)
                 * list of messages of the same type (say, ack everyone after we are
                 * done with the last one). */
                lk_lock(lk_ipcMessageQueue, 1);
                for (int unblock_asid : ack_list) {
                    for (auto ack_it = ackMessages->begin(); ack_it != ackMessages->end();
                         ++ack_it) {
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
