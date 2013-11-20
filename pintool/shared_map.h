// Wrapper class for Boost's interprocess map that handles allocation, mapping,
// and expansion of the shared memory segment.
//
// This class does not provide an STL-compliant interface. All operations must
// be performed through insert() and at(). insert(key, value) inserts this
// key-value pair if key does not exist in the map and updates the value stored
// at key if key does already exist. at() returns a const reference to an
// existing value in the map. begin() and end() iterators are provided as well,
// but they are boost iterators rather than STL iterators.
//
// Usage of this wrapper requires that the caller must have the shared memory
// segment mapped in its scope. This can be accomplished using a global pointer
// that does not change.
//
// Dynamic growth of the shared memory segment is not supported through this
// library. Expect segmentation faults should the memory segment be grown. This
// library asserts false on an insert if the shared segment is out of memory.
//
// This does not guarantee that any classes or data structures inserted into the
// map will be stored in shared memory. Applications that require this
// functionality must define their own allocators.
//
// Example:
//   managed_shared_memory shm(create_only, "shared_memory_key", 65536);
//   SharedMemoryMap<int, int> map("shared_memory_key", "shared_data_key");
//   map.insert(10, 10);
//   map.insert(11, 15);
//   int value = map.at(11);  // returns 15
//
//   map.at(11) = 16;
//   value = map.at(11);  // returns 16.
//
// To use strings, vectors, or other containers that allocate memory themselves,
// see the Boost interprcess documentation at
// http://www.boost.org/doc/libs/1_54_0/doc/html/interprocess.html.
//
// Author: Sam Xi

#ifndef SHARED_MEMORY_MAP_H
#define SHARED_MEMORY_MAP_H

#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <cstdlib>  // for size_t
#include <functional>  // for std::less
#include <utility>  // for std::pair
#include <iostream>

#include "shared_common.h"

namespace xiosim {
namespace shared {

using namespace boost::interprocess;

template<typename K, typename V>
class SharedMemoryMap : public AbstractSharedMemoryData {
  public:
    typedef std::pair<const K, V> MapValueType;

    SharedMemoryMap() {}

    // Initializes the size of the shared memory segment holding this shared map
    SharedMemoryMap(
        const char* shared_memory_name, const char* internal_map_name)
      : AbstractSharedMemoryData(shared_memory_name, internal_map_name) {
      initialize();
    }

    // Destroys the pointer to the shared map.
    // TODO: Only delete if no processes have mapped this structure.
    ~SharedMemoryMap() {}

    void initialize_late(
        const char* shared_memory_name, const char* internal_map_name) {
      memory_key = boost::interprocess::string(shared_memory_name);
      data_key = boost::interprocess::string(internal_map_name);
      initialize();
    }

    // Inserts value into this map if key does not already exist, and returns
    // the key-value of the newly inserted pair or that of the existing
    // key-value pair.
    // If the shared memory segment runs out of free space, this will not
    // attempt to grow the segment, but instead will assert false.
    void insert(K key, V value) {
      MapValueType new_pair(key, value);
      try {
        MapValueAllocator alloc_inst(shm->get_segment_manager());
        internal_map = shm->find_or_construct<InternalMap>(data_key.c_str())(
            std::less<K>(), alloc_inst);
        internal_map->insert(new_pair);
      } catch (const boost::interprocess::bad_alloc&) {
        check_and_grow_shared_memory();
        insert(key, value);
      }
    }

    // Returns a const reference to the value stored at key. If the key-value
    // pair does not exist, an out-of-range exception is thrown.
    V& at(K key) {
      return internal_map->at(key);
    }

    std::size_t size() {
      return internal_map->size();
    }

    typename boost::interprocess::map<K, V>::iterator begin() {
      return internal_map->begin();
    }

    typename boost::interprocess::map<K, V>::iterator end() {
      return internal_map->end();
    }

  private:
    typedef boost::interprocess::allocator<
        MapValueType, managed_shared_memory::segment_manager> 
          MapValueAllocator;
    typedef boost::interprocess::map<K, V, std::less<K>, MapValueAllocator> 
        InternalMap;

    // Boost interprocess map that contains the actual data.
    InternalMap *internal_map;

    void initialize() {
      // TODO: Track the number of processes that have mapped this object.
      MapValueAllocator alloc_inst(shm->get_segment_manager());
      internal_map = shm->find_or_construct<InternalMap>(data_key.c_str())(
          std::less<K>(), alloc_inst);
    }
};

}  // namespace shared
}  // namespace xiosim

#endif
