#ifndef __BUFFER_MANAGER_PRODUCER__
#define __BUFFER_MANAGER_PRODUCER__

#include "BufferManager.h"

namespace xiosim
{
namespace buffer_management
{

void InitBufferManagerProducer(pid_t harness_pid, int num_cores);
void DeinitBufferManagerProducer(void);

void AllocateThreadProducer(pid_t tid);
// The two steps of a push -- get a buffer, do magic with
// it, and call producer_done once it can be consumed / flushed
// In between, back() will return a pointer to that buffer
handshake_container_t* get_buffer(pid_t tid);
// By assumption, we call producer_done() once we have a completely
// instrumented, valid handshake, so that we don't need to handle
// intermediate cases
void producer_done(pid_t tid, bool keepLock=false);

void flushBuffers(pid_t tid);

handshake_container_t* back(pid_t tid);

void resetPool(pid_t tid);
}
}

#endif /* __BUFFER_MANAGER_PRODUCER__ */
