#ifndef __BUFFER_MANAGER_PRODUCER__
#define __BUFFER_MANAGER_PRODUCER__

#include <string>

#include "BufferManager.h"
#include "handshake_container.h"

namespace xiosim {
namespace buffer_management {

/* Pushing and popping produceBuffer_: */
/* The two steps of a push -- get a buffer, do magic with
 * it, and call ProducerDone once it can be consumed / flushed
 * In between, Back() will return a pointer to that buffer. */
handshake_container_t* GetBuffer(pid_t tid);
/* By assumption, we call ProducerDone() once we have a completely
 * instrumented, valid handshake, so that we don't need to handle
 * intermediate cases.
 * If produceBuffer_ becomes full, we will flush it out to fileBuffer_. */
void ProducerDone(pid_t tid, bool keepLock = false);
/* Get a pointer to the last element of produceBuffer_. */
handshake_container_t* Back(pid_t tid);

/* Flush everything in produceBuffer_ to fileBuffer_ so it can
 * be consumed straight away. */
void FlushBuffers(pid_t tid);

/* Any elements in the current produceBuffer_? */
bool ProducerEmpty(pid_t tid);

/* Init producerBuffer_ structures. */
void InitBufferManagerProducer(pid_t harness_pid, bool skip_space_check, std::string bridge_dirs);
/* Cleanup. */
void DeinitBufferManagerProducer(void);
/* Allocate produceBuffer_ for a new program thread. */
void AllocateThreadProducer(pid_t tid);
}
}

#endif /* __BUFFER_MANAGER_PRODUCER__ */
