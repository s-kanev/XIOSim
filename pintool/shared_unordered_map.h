// A wrapper class for Boost's unordered map that handles allocation and mapping
// of values into a shared memory segment.
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
// Mutator methods: insert() and operator[].
// Access methods: at().
// This library does not provide an STL-compliant interface.
//
// Author: Sam Xi

#ifndef SHARED_UNORDERED_MAP_H
#define SHARED_UNORDERED_MAP_H

#include "shared_unordered_common.h"

namespace xiosim {
namespace shared {

template<typename K, typename V, typename enable = void>
class SharedUnorderedMap : public SharedUnorderedMapCommon<K, V> {
  typedef SharedUnorderedMapCommon<K, V> ShmCommon;
  using ShmCommon::shm;
  using ShmCommon::internal_map;

  public:
    // Initializes the size of the shared memory segment holding this shared map
    SharedUnorderedMap(
        const char* shared_memory_name, const char* internal_data_name,
        std::size_t buckets = ShmCommon::DEFAULT_BUM_BUCKETS)
        : ShmCommon(shared_memory_name, internal_data_name, buckets) {}

        SharedUnorderedMap() : ShmCommon() {}

  private:
    typedef boost::interprocess::allocator<
        void, managed_shared_memory::segment_manager> void_allocator;

    V& access_operator(const K& key) {
      if (internal_map->find(key) == internal_map->end()) {
        void_allocator alloc_inst(shm->get_segment_manager());
        V value(alloc_inst);
        this->insert(key, value);
      }
      return internal_map->at(key);
    }
};

//Default constructing value generates a warning on some GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

// Specialization for plain-old-data (POD) types of values.
template<typename K, typename V>
class SharedUnorderedMap<
    K, V, typename boost::enable_if<boost::is_pod<V> >::type> :
    public SharedUnorderedMapCommon<K, V> {
  typedef SharedUnorderedMapCommon<K, V> ShmCommon;
  using ShmCommon::shm;
  using ShmCommon::internal_map;

  public:
    SharedUnorderedMap() : ShmCommon() {}

    // Initializes the size of the shared memory segment holding this shared map
    SharedUnorderedMap(
        const char* shared_memory_name, const char* internal_data_name,
        std::size_t buckets = ShmCommon::DEFAULT_BUM_BUCKETS)
        : ShmCommon(shared_memory_name, internal_data_name, buckets) {}

  private:
    V& access_operator(const K& key) {
      if (internal_map->find(key) == internal_map->end()) {
        V value;
        this->insert(key, value);
      }
      return internal_map->at(key);
    }
};

#pragma GCC diagnostic pop

}
}

#endif
