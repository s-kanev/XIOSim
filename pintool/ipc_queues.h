#ifndef __IPC_QUEUES__
#define __IPC_QUEUES__

struct ipc_message_t;

/* Send an IPC message. The flow now is from multiple producers
 * (different feeders, harness) to possible multiple consumers
 * (threads inside timing_sim). For now, we can get message responses
 * only by blocking messages.
 * XXX: We are assuming only one blocking message with a certain
 * type and parameters at a time. Otherwise, the blocking mechanism
 * will fail silently and graciously. */
void SendIPCMessage(ipc_message_t msg, bool blocking = false);

/* Consume messages from IPC queue until empty.
 * @isEarly selects the right queue based on the caller site
 * (some messages must be consumed without waiting for cycle advances).
 * Blocking messages are ack-ed after processing (which can take a while).
 */
void CheckIPCMessageQueue(bool isEarly, int caller_coreID);

/* Map IPC queues in each process' address space. Should be called
 * by any process that want to send/receive IPC messages. */
void InitIPCQueues(void);

enum ipc_message_id_t { SLICE_START, SLICE_END, MMAP, MUNMAP, UPDATE_BRK, WARM_LLC, STOP_SIMULATION, ACTIVATE_CORE, DEACTIVATE_CORE, SCHEDULE_NEW_THREAD, ALLOCATE_THREAD, THREAD_AFFINITY, ALLOCATE_CORES, DEALLOCATE_CORES, INVALID_MSG };

struct ipc_message_t {
    ipc_message_id_t id;
    int64_t arg0;
    int64_t arg1;
    int64_t arg2;
    int64_t arg3;

    /* Does sender wait until message has been *processed*,
     * not only consumed. */
    bool blocking;

    ipc_message_t() : 
        id(INVALID_MSG),
        arg0(0), arg1(0), arg2(0), arg3(0),
        blocking(false) {}

    /* Some messages need to be conusmed early in timing_sim.
     * Mostly related to setup. */
    bool ConsumableEarly() const {
        switch (this->id) {
          case SLICE_START:
          case SLICE_END:
          case ACTIVATE_CORE:
          case ALLOCATE_THREAD:
          case SCHEDULE_NEW_THREAD:
          case STOP_SIMULATION:
          case THREAD_AFFINITY:
            return true;
          default:
            return false;
        }
    }

    void SliceStart(unsigned int slice_num)
    {
        this->id = SLICE_START;
        this->arg0 = slice_num;
    }

    void SliceEnd(unsigned int slice_num, unsigned long long feeder_slice_length, unsigned long long slice_weight_times_1000)
    {
        this->id = SLICE_END;
        this->arg0 = slice_num;
        this->arg1 = feeder_slice_length;
        this->arg2 = slice_weight_times_1000;
    }

    void Mmap(int asid, unsigned int addr, unsigned int length, bool mod_brk)
    {
        this->id = MMAP;
        this->arg0 = asid;
        this->arg1 = addr;
        this->arg2 = length;
        this->arg3 = mod_brk;
    }

    void Munmap(int asid, unsigned int addr, unsigned int length, bool mod_brk)
    {
        this->id = MUNMAP;
        this->arg0 = asid;
        this->arg1 = addr;
        this->arg2 = length;
        this->arg3 = mod_brk;
    }

    void UpdateBrk(int asid, unsigned int brk_end, bool do_mmap)
    {
        this->id = UPDATE_BRK;
        this->arg0 = asid;
        this->arg1 = brk_end;
        this->arg2 = do_mmap;
    }

    void StopSimulation(bool kill_sim_threads)
    {
        this->id = STOP_SIMULATION;
        this->arg0 = kill_sim_threads;
    }

    void ActivateCore(int coreID)
    {
        this->id = ACTIVATE_CORE;
        this->arg0 = coreID;
    }

    void DeactivateCore(int coreID)
    {
        this->id = DEACTIVATE_CORE;
        this->arg0 = coreID;
    }

    void ScheduleNewThread(int tid)
    {
        this->id = SCHEDULE_NEW_THREAD;
        this->arg0 = tid;
    }

    void BufferManagerAllocateThread(int tid, int buffer_capacity)
    {
        this->id = ALLOCATE_THREAD;
        this->arg0 = tid;
        this->arg1 = buffer_capacity;
    }

    void SetThreadAffinity(int tid, int coreID)
    {
        this->id = THREAD_AFFINITY;
        this->arg0 = tid;
        this->arg1 = coreID;
    }

    void AllocateCores(int asid, const char* name)
    {
        this->id = ALLOCATE_CORES;
        this->arg0 = asid;
    }

    void DeallocateCores(int asid)
    {
        this->id = DEALLOCATE_CORES;
        this->arg0 = asid;
    }

    bool operator==(const ipc_message_t &rhs) const {
        return (this->id == rhs.id) &&
                (this->arg0 == rhs.arg0) &&
                (this->arg1 == rhs.arg1) &&
                (this->arg2 == rhs.arg2);
                
    }
};
static size_t hash_value(ipc_message_t const& obj)
{
    boost::hash<int> hasher;
    return hasher(obj.id) ^ hasher(obj.arg0);
}

/* Message queue form calling functions in timing_sim from feeder */
typedef boost::interprocess::allocator<ipc_message_t, boost::interprocess::managed_shared_memory::segment_manager> ipc_message_allocator_t;

typedef boost::interprocess::deque<ipc_message_t, ipc_message_allocator_t> MessageQueue;
SHARED_VAR_DECLARE(MessageQueue, ipcMessageQueue);
SHARED_VAR_DECLARE(MessageQueue, ipcEarlyMessageQueue);
/* A response queue to acknowledge recieving blocking messages. */
typedef xiosim::shared::SharedUnorderedMap<ipc_message_t, bool> AckMessageMap;
SHARED_VAR_DECLARE(AckMessageMap, ackMessages)
SHARED_VAR_DECLARE(XIOSIM_LOCK, lk_ipcMessageQueue);

extern XIOSIM_LOCK *printing_lock;

#endif /* __IPC_QUEUES__ */
