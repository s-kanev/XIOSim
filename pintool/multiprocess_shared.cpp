#include "multiprocess_shared.h"

using namespace xiosim::shared;

boost::interprocess::managed_shared_memory *global_shm;

SHARED_VAR_DEFINE(bool, consumers_sleep)
SHARED_VAR_DEFINE(bool, producers_sleep)
SHARED_VAR_DEFINE(BufferManager, handshake_buffer)
SHARED_VAR_DEFINE(PIN_SEMAPHORE, consumer_sleep_lock);
SHARED_VAR_DEFINE(PIN_SEMAPHORE, producer_sleep_lock);

SHARED_VAR_DEFINE(MessageQueue, ipcMessageQueue);
SHARED_VAR_DEFINE(XIOSIM_LOCK, lk_ipcMessageQueue);

KNOB<int> KnobNumProcesses(KNOB_MODE_WRITEONCE,      "pintool",
        "num_processes", "1", "Number of processes for a multiprogrammed workload");

void InitSharedState(bool wait_for_others)
{
    using namespace boost::interprocess;
    int *process_counter;

    named_mutex init_lock(open_only, XIOSIM_INIT_SHARED_LOCK);
    global_shm = new managed_shared_memory(open_or_create, XIOSIM_SHARED_MEMORY_KEY,
            DEFAULT_SHARED_MEMORY_SIZE);
    init_lock.lock();

    if (wait_for_others) {
        process_counter = global_shm->find_or_construct<int>(XIOSIM_INIT_COUNTER_KEY)(
            KnobNumProcesses.Value());
        std::cout << "Counter value is: " << *process_counter << std::endl;
        (*process_counter)--;
    }

    SHARED_VAR_INIT(bool, producers_sleep, false)
    SHARED_VAR_INIT(bool, consumers_sleep, false)
    SHARED_VAR_INIT(BufferManager, handshake_buffer)
    SHARED_VAR_INIT(PIN_SEMAPHORE, consumer_sleep_lock);
    SHARED_VAR_INIT(PIN_SEMAPHORE, producer_sleep_lock);

    SHARED_VAR_INIT(MessageQueue, ipcMessageQueue);
    SHARED_VAR_INIT(XIOSIM_LOCK, lk_ipcMessageQueue);

    PIN_SemaphoreInit(consumer_sleep_lock);
    PIN_SemaphoreInit(producer_sleep_lock);

    init_lock.unlock();

    if (wait_for_others) {
        // Spin until the counter reaches zero, indicating that all other processes
        // have reached this point.
        while (1) {
            init_lock.lock();
            if (*process_counter == 0) {
                init_lock.unlock();
                break;
            }
            init_lock.unlock();
        }
    }
    std::cout << "Proceeeding to execute pintool.\n";
}

void SendIPCMessage(ipc_message_t msg)
{
    lk_lock(lk_ipcMessageQueue, 1);
    ipcMessageQueue->push_back(msg);
    lk_unlock(lk_ipcMessageQueue);
}
