// An example to demonstrate the use of a shared memory map across 10 processes
// aech running 10 threads.
//
// To compile, you must link with the boost libraries and with the mpkeys_impl.o
// object file.
//
// Author: Sam Xi

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <utility>
#include <functional>  // for less
#include <pthread.h>
#include <string>
#include <time.h>
#include <sstream>

#include "shared_map.h"
#include "shared_unordered_map.h"

using namespace boost::interprocess;
using namespace xiosim::shared;

// A simple struct to experiment with.
struct mystruct_t {
  int value;
  mystruct_t(int v) { value = v; };
  mystruct_t(const mystruct_t &m) { value = m.value; };
};

// Typedefs for shared map
typedef int KeyType;
typedef mystruct_t ValueType;
typedef std::pair<const KeyType, ValueType> MapValueType;
typedef allocator<MapValueType, managed_shared_memory::segment_manager>
    MapValueAllocator;
typedef map<KeyType, ValueType, std::less<KeyType>, MapValueAllocator> ShmMap;
typedef allocator<char, managed_shared_memory::segment_manager> CharAllocator;
typedef basic_string<char, std::char_traits<char>, CharAllocator> ShmString;
typedef allocator<ShmString, managed_shared_memory::segment_manager>
    StringAllocator;
typedef allocator<void, managed_shared_memory::segment_manager> VoidAllocator;
typedef allocator<mystruct_t, managed_shared_memory::segment_manager>
    StructAllocator;

// Mutexes
pthread_mutex_t cout_lock;
pthread_mutex_t update_lock;

// Global pointer to shared memory segment.
managed_shared_memory *global_shm;

// Global names
const char *SHARED_MEMORY_NAME = "MySharedMemory";
const char *SHARED_MAP_NAME = "map_key";
const int DEFAULT_SIZE = 65536;

// Global pointer to shared map.
SharedMemoryMap<int, ShmString> *mymap;

// Entry point for pthreads created by the child process. Each thread attempts
// to write 100 values to 100 keys in the shared map.
void* myfunction(void *arg) {
  int value = *((int*) arg);
  VoidAllocator alloc_inst = global_shm->get_allocator<void>();
  for (int i = 0; i < 100; i++) {
    std::stringstream stored_str;
    int lock_status = pthread_mutex_lock(&cout_lock);

    stored_str << "something from iteration " << i << " of thread " << value;
    ShmString mystring(stored_str.str().c_str(), alloc_inst);
    // ShmString mystring("something", alloc_inst);
    std::cout << "Thread " << value << " is inserting at key " << i <<
        std::endl;
    // mymap->insert(i, mystring);
    mymap->operator[](i) = mystring;

    pthread_mutex_unlock(&cout_lock);
  }
  return NULL;
}

// Function that gets run by the child process.
int multithreaded() {
  pthread_mutex_init(&cout_lock, NULL);
  pthread_mutex_init(&update_lock, NULL);
  int num_threads = 3;
  void *status;
  pthread_t threads[num_threads];
  int values[10];
  // Fire off 10 threads and let them do their work.
  for (int i = 0; i < num_threads; i++) {
    values[i] = i;
    pthread_create(&threads[i], NULL, myfunction, &values[i]);
  }
  // Wait for all of them to finish.
  for (int i = 0; i < num_threads; i++) {
    pthread_join(threads[i], &status);
    int lock_status = pthread_mutex_lock(&cout_lock);
    std::cout << "Thread " << i << " finished with status " << status <<
        std::endl;
    pthread_mutex_unlock(&cout_lock);
  }

  int lock_status = pthread_mutex_lock(&cout_lock);
  std::cout << "Size of the map is " << mymap->size() << std::endl;
  pthread_mutex_unlock(&cout_lock);
  return 0;
}

int main() {
  struct shm_remove {
    shm_remove() { shared_memory_object::remove(SHARED_MEMORY_NAME); }
    ~shm_remove() { shared_memory_object::remove(SHARED_MEMORY_NAME); }
  } remover;

  // Initialize the global pointer to the shared segment and map.
  global_shm = new managed_shared_memory(
      open_or_create, SHARED_MEMORY_NAME, DEFAULT_SIZE);
  mymap = new SharedMemoryMap<int, ShmString>(SHARED_MEMORY_NAME,
      SHARED_MAP_NAME);

  pid_t pid = fork();
  int status;
  switch (pid) {
    case 0:  { // child
      multithreaded();
      exit(1);
      break;
    }
    case -1:  { // error
      perror("Fork failed.");
      break;
    }
    default:  { // parent
      wait(&status);
      std::cout<< "Child process exited with status " << status << std::endl;
      std::cout << "Parent sees size of map as: " << mymap->size() <<
          std::endl;
      // Dump the contents of the map.
      for (int i = 0; i < mymap->size(); i ++) {
        std::cout << "Value at " << i << " is " << mymap->at(i) << std::endl;
      }
      exit(0);
      break;
    }
  }
}
