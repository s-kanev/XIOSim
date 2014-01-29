#ifndef __IPC_QUEUES__
#define __IPC_QUEUES__

/* Message queue form calling functions in timing_sim from feeder */
typedef boost::interprocess::allocator<ipc_message_t, boost::interprocess::managed_shared_memory::segment_manager> ipc_message_allocator_t;
typedef boost::interprocess::deque<ipc_message_t, ipc_message_allocator_t> MessageQueue;

SHARED_VAR_DECLARE(MessageQueue, ipcMessageQueue);
SHARED_VAR_DECLARE(MessageQueue, ipcEarlyMessageQueue);
SHARED_VAR_DECLARE(XIOSIM_LOCK, lk_ipcMessageQueue);

extern XIOSIM_LOCK *printing_lock;

void SendIPCMessage(ipc_message_t msg);
void InitIPCQueues(void);

#endif /* __IPC_QUEUES__ */
