// Demonstrates atomically decrementing a counter in shared memory between
// multiple processes using boost named_mutex and interprocess APIs.
//
// To run:
// ./harness -num_processes 2 ./harness_example
//
// NOTE: This code is obsolete, as harness now reads from a configuration file
// instead.
//
// Author: Sam Xi

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>

#include "mpkeys.h"

int main() {

  using namespace boost::interprocess;
  using namespace xiosim::shared;

  managed_shared_memory shm(open_or_create, XIOSIM_SHARED_MEMORY_KEY,
      DEFAULT_SHARED_MEMORY_SIZE);
  std::cout << "Opened shared memory" << std::endl;
  named_mutex init_lock(open_only, XIOSIM_INIT_SHARED_LOCK);
  std::cout << "Opened lock" << std::endl;
  init_lock.lock();
  std::cout << "Lock acquired" << std::endl;

  int *counter = shm.find_or_construct<int>(XIOSIM_INIT_COUNTER_KEY)(0);
  std::cout << "Counter value is: " << *counter << std::endl;
  (*counter)--;
  init_lock.unlock();
  while (*counter > 0);
  std::cout << "COntinuing execution." << std::endl;

  return 0;
}
