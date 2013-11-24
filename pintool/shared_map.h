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
#include <boost/type_traits/is_pod.hpp>
#include <boost/utility/enable_if.hpp>
#include <cstdlib>  // for size_t
#include <functional>  // for std::less
#include <utility>  // for std::pair
#include <iostream>

#include "shared_common.h"

namespace xiosim {
namespace shared {

using namespace boost::interprocess;

template<typename K, typename V, typename enable = void>
class SharedMemoryMap : public SharedMemoryMapCommon<K, V> {
  using SharedMemoryMapCommon<K, V>::shm;
  using SharedMemoryMapCommon<K, V>::internal_map;

  public:
    SharedMemoryMap() : SharedMemoryMapCommon<K, V>() {}

    // Initializes the size of the shared memory segment holding this shared map
    SharedMemoryMap(
        const char* shared_memory_name, const char* internal_data_name)
        : SharedMemoryMapCommon<K, V>(shared_memory_name, internal_data_name) {}

    // Destroys the pointer to the shared map.
    // TODO: Only delete if no processes have mapped this structure.
    ~SharedMemoryMap() {}

  private:
    typedef boost::interprocess::allocator<void, managed_shared_memory::segment_manager>
        void_allocator;

    V& access_operator(const K& key) {
      if (internal_map->find(key) == internal_map->end()) {
        void_allocator alloc_inst(shm->get_segment_manager());
        V value(alloc_inst);
        this->insert(key, value);
      }
      return internal_map->at(key);
    }

};

// A specialization for plain-old-data (POD) types of values.
template<typename K, typename V>
class SharedMemoryMap<K, V, typename boost::enable_if<boost::is_pod<V> >::type> :
    public SharedMemoryMapCommon<K, V> {
  using SharedMemoryMapCommon<K, V>::shm;
  using SharedMemoryMapCommon<K, V>::internal_map;

  public:
    SharedMemoryMap() : SharedMemoryMapCommon<K, V>() {}

    // Initializes the size of the shared memory segment holding this shared map
    SharedMemoryMap(
        const char* shared_memory_name, const char* internal_data_name)
        : SharedMemoryMapCommon<K, V>(shared_memory_name, internal_data_name) {}

    // Destroys the pointer to the shared map.
    // TODO: Only delete if no processes have mapped this structure.
    ~SharedMemoryMap() {}

  private:
    V& access_operator(const K& key) {
      if (internal_map->find(key) == internal_map->end()) {
        V value;
        this->insert(key, value);
      }
      return internal_map->at(key);
    }
};

}  // namespace shared
}  // namespace xiosim

#endif
