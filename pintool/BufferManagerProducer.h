#ifndef __BUFFER_MANAGER_PRODUCER__
#define __BUFFER_MANAGER_PRODUCER__

#include "BufferManager.h"

namespace xiosim
{
namespace buffer_management
{

void InitBufferManagerProducer(void);
void DeinitBufferManagerProducer(void);

void AllocateThreadProducer(THREADID tid);
// The two steps of a push -- get a buffer, do magic with
// it, and call producer_done once it can be consumed / flushed
// In between, back() will return a pointer to that buffer
handshake_container_t* get_buffer(THREADID tid);
// By assumption, we call producer_done() once we have a completely
// instrumented, valid handshake, so that we don't need to handle
// intermediate cases
void producer_done(THREADID tid, bool keepLock=false);

void flushBuffers(THREADID tid);

handshake_container_t* back(THREADID tid);

void resetPool(THREADID tid);
}
}

#endif /* __BUFFER_MANAGER_PRODUCER__ */
