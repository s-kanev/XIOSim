#ifndef __BUFFER_MANAGER_CONSUMER__
#define __BUFFER_MANAGER_CONSUMER__

#include "handshake_container.h"

namespace xiosim {
namespace buffer_management {
/* Pushing and popping consumeBuffer_: */
/* Get the number of etnries in consumeBuffer_, so we can process them in bulk. */
extern int GetConsumerSize(pid_t tid);
/* Get the head of consumeBuffer_. If empty, it will reach out to fileBuffer_,
 * where the thread can wait until an entry becomes available. */
extern handshake_container_t* Front(pid_t tid);
/* Invalidate the head of conusmeBuffer_. Move on to the next entry. */
extern void Pop(pid_t tid);

/* Init consumeBuffer_ structures. */
extern void InitBufferManagerConsumer(pid_t harness_pid);
/* Cleanup. */
extern void DeinitBufferManagerConsumer();
/* Allocate consumeBuffer_ for a new program thread. */
extern void AllocateThreadConsumer(pid_t tid, int buffer_capacity);
}
}

#endif /* __BUFFER_MANAGER_CONSUMER__ */
