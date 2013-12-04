#ifndef __BUFFER_MANAGER_CONSUMER__
#define __BUFFER_MANAGER_CONSUMER__

#include "BufferManager.h"

namespace xiosim
{
namespace buffer_management
{
void InitBufferManagerConsumer();
void DeinitBufferManagerConsumer();
void AllocateThreadConsumer(THREADID tid, int buffer_capacity);

handshake_container_t* front(THREADID tid, bool isLocal=false);
void pop(THREADID tid);
void applyConsumerChanges(THREADID tid, int numChanged);
int getConsumerSize(THREADID tid);
}
}

#endif /* __BUFFER_MANAGER_CONSUMER__ */
