// Parent class of all shared memory data structure wrappers.
//
// All classes inheriting this class must implement a private initialize()
// method in addition to the standard data manipulation methods.
//
// Author: Sam Xi

#ifndef ABSTRACT_SHARED_MEMORY_DATA_H
#define ABSTRACT_SHARED_MEMORY_DATA_H

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstdlib>  // for size_t
#include <utility>  // for std::pair

namespace xiosim {
namespace shared {

using namespace boost::interprocess;

// When the shared memory segment's free space is insufficient, upon the next
// insert/update operation, expand it by this many bytes.
const std::size_t DEFAULT_GROW_MEMORY_SIZE = 65536;

class AbstractSharedMemoryData {

  public:
    typedef std::pair<std::size_t, std::size_t> MemorySizeStateType;
    typedef boost::interprocess::allocator<void, managed_shared_memory::segment_manager>
        VoidAllocator;

    AbstractSharedMemoryData() {}

    AbstractSharedMemoryData(
        const char* shared_memory_name, const char* internal_data_name)
        : data_key(internal_data_name), memory_key(shared_memory_name) {

      shm = new managed_shared_memory(
          open_or_create, memory_key.c_str(), DEFAULT_SHARED_MEMORY_SIZE);
    }

  protected:
    // Name that identifies this map in the shared memory segment.
    boost::interprocess::string data_key;
    // Name that identifies this shared memory segment.
    boost::interprocess::string memory_key;
    // Shared memory segment. This must always be mapped into memory in order to
    // maintain consistency.
    managed_shared_memory *shm;

    // Initializes the actual data structure in shared memory.
    virtual void initialize() = 0;

    // Returns a std::pair<size_t, size_t> tuple. The first element is the total
    // size of the shared memory segment, and the second element is the amount
    // of free memory remaining.
    MemorySizeStateType get_state_of_memory() {
      MemorySizeStateType size(shm->get_size(), shm->get_free_memory());
      return size;
    }

    // Increases the size of the shared memory segment.
    // TODO: This must be synchronized so that no other process is mapping the
    // segment while this operation takes place.
    void check_and_grow_shared_memory() {
      // For now, growing shared memory is really messy. Let's just make the
      // segment big enough so that we don't ever need to.
      assert(false);

      // But if we wanted to grow memory, the code would look something like
      // this.
      MemorySizeStateType shm_state = get_state_of_memory();
      delete shm;
      managed_shared_memory::grow(
          memory_key.c_str(), DEFAULT_GROW_MEMORY_SIZE);
      shm = new managed_shared_memory(open_only, memory_key.c_str());
      MemorySizeStateType new_shm_state = get_state_of_memory();
      assert(
          shm_state.first + DEFAULT_GROW_MEMORY_SIZE == new_shm_state.first);
    }
};

}  // namespace shared
}  // namespace xiosim

#endif
