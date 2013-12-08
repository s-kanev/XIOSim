#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>

#include "pin.H"

#include "shared_map.h"
#include "shared_unordered_map.h"

#include "../interface.h"
#include "multiprocess_shared.h"
#include "ipc_queues.h"

SHARED_VAR_DEFINE(MessageQueue, ipcMessageQueue)
SHARED_VAR_DEFINE(MessageQueue, ipcEarlyMessageQueue)
SHARED_VAR_DEFINE(XIOSIM_LOCK, lk_ipcMessageQueue)

void InitIPCQueues(void) {
    ipc_message_allocator_t *ipc_queue_allocator = new ipc_message_allocator_t(global_shm->get_segment_manager());

    SHARED_VAR_INIT(MessageQueue, ipcMessageQueue, *ipc_queue_allocator);
    SHARED_VAR_INIT(MessageQueue, ipcEarlyMessageQueue, *ipc_queue_allocator);
    SHARED_VAR_INIT(XIOSIM_LOCK, lk_ipcMessageQueue);
}

void SendIPCMessage(ipc_message_t msg)
{
    MessageQueue *q = msg.ConsumableEarly() ? ipcEarlyMessageQueue : ipcMessageQueue;

#ifdef IPC_DEBUG
    lk_lock(printing_lock, 1);
    std::cerr << "[SEND] IPC message, ID: " << msg.id << std::endl;
    lk_unlock(printing_lock);
#endif

    lk_lock(lk_ipcMessageQueue, 1);
    q->push_back(msg);
    lk_unlock(lk_ipcMessageQueue);
}

