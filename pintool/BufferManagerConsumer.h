#ifndef __BUFFER_MANAGER_CONSUMER__
#define __BUFFER_MANAGER_CONSUMER__

#include "BufferManager.h"

namespace xiosim
{
namespace buffer_management
{
void InitBufferManagerConsumer(pid_t harness_pid, int num_cores);
void DeinitBufferManagerConsumer();
void AllocateThreadConsumer(pid_t tid, int buffer_capacity);

handshake_container_t* front(pid_t tid, bool isLocal=false);
void pop(pid_t tid);
void applyConsumerChanges(pid_t tid, int numChanged);
int getConsumerSize(pid_t tid);
}
}

#endif /* __BUFFER_MANAGER_CONSUMER__ */
