#ifndef HANDSHAKE_BUFFER_MANAGER
#define HANDSHAKE_BUFFER_MANAGER

#include <utility>
#include <string>

namespace xiosim {
namespace buffer_management {
/* Handshake buffer interface. The connection between producers of architected
 * state (feeders) and consumers (simulated cores). There is a FIFO buffer per
 * simulated thread.

 * The overall structure of the buffer is as follows:
 * ]-------------] ]-------------------] ]-------------]
 * produceBuffer_       fileBuffer      consumeBuffer_
 *

 * Feeders write to produceBuffer_ without capturing any locks,
 * and conusmers read from consumeBuffers_ without synchronization either.
 * When producerBuffer_ gets full, we dump to one entry in fileBuffer,
 * which usually lives in /dev/shm. When consumeBuffer_ is empty, it
 * waits until an entry in fileBuffer shows up.
 */

/* Pushing and popping fileBuffer: */
/* On the producer side, once we have written a fileBuffer entry, make
 * it visible to the consumer. */
extern void NotifyProduced(pid_t tid, std::string filename, size_t n_items);
/* On the consumer side, wait until a file in fileBuffer shows up.
 * The thread will sleep on a cv while fileBuffer is empty.
 * Returns the filename and number of handshakes to read from the file. */
extern std::pair<std::string, size_t> WaitForFile(pid_t tid);
/* On the consumer side, notify that we've read @n_items from the head
 * of fileBuffer.
 * XXX: If thead contention becomes an issue, we can fold this in WaitForFile. */
extern void NotifyConsumed(pid_t tid, size_t n_items);

/* Init the shared memory structures. */
extern void InitBufferManager(pid_t harness_pid, int num_cores);
/* Nada. */
extern void DeinitBufferManager();
/* Allocate fileBuffer for a new program thread. */
extern int AllocateThread(pid_t tid);
}
}

#endif
