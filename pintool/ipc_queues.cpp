#include "pin.H"

#include "boost_interprocess.h"

#include "../interface.h"
#include "multiprocess_shared.h"
#include "ipc_queues.h"

SHARED_VAR_DEFINE(MessageQueue, ipcMessageQueue)
SHARED_VAR_DEFINE(MessageQueue, ipcEarlyMessageQueue)
SHARED_VAR_DEFINE(AckMessageMap, ackMessages)
SHARED_VAR_DEFINE(XIOSIM_LOCK, lk_ipcMessageQueue)

void InitIPCQueues(void) {
    ipc_message_allocator_t* ipc_queue_allocator =
        new ipc_message_allocator_t(global_shm->get_segment_manager());

    SHARED_VAR_INIT(MessageQueue, ipcMessageQueue, *ipc_queue_allocator);
    SHARED_VAR_INIT(MessageQueue, ipcEarlyMessageQueue, *ipc_queue_allocator);
    SHARED_VAR_INIT(XIOSIM_LOCK, lk_ipcMessageQueue);

    SHARED_VAR_CONSTRUCT(AckMessageMap, ackMessages);
}

void SendIPCMessage(ipc_message_t msg, bool blocking) {
    MessageQueue* q = msg.ConsumableEarly() ? ipcEarlyMessageQueue : ipcMessageQueue;
    msg.blocking = blocking;

#ifdef IPC_DEBUG
    lk_lock(printing_lock, 1);
    std::cerr << "[SEND] IPC message, ID: " << msg.id << std::endl;
    lk_unlock(printing_lock);
#endif

    lk_lock(lk_ipcMessageQueue, 1);
    q->push_back(msg);

    if (blocking)
        ackMessages->operator[](msg) = false;
    lk_unlock(lk_ipcMessageQueue);

    if (blocking) {
        lk_lock(lk_ipcMessageQueue, 1);
        while (ackMessages->at(msg) == false) {
            lk_unlock(lk_ipcMessageQueue);
            xio_sleep(10);
            lk_lock(lk_ipcMessageQueue, 1);
        }
        lk_unlock(lk_ipcMessageQueue);
    }
}
