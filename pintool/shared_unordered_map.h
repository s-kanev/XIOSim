#ifndef SHARED_UNORDERED_MAP
#define SHARED_UNORDERED_MAP

#include <boost/functional/hash.hpp>  // boost::hash
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/unordered_map.hpp>

#include "shared_map.h"

#include <cstdlib>  // for size_t
#include <functional>  // for std::equal_to
#include <utility>  // for std::pair
#include <iostream>


namespace xiosim {
namespace shared {

using namespace boost::interprocess;

const std::size_t DEFAULT_NUM_BUCKETS = 16;

template<typename K, typename V>
class SharedUnorderedMap : public SharedMemoryMap<K, V> {

  public:
    SharedUnorderedMap() {}

    SharedUnorderedMap(
        const char* shared_memory_name, const char* internal_map_name,
        std::size_t buckets = DEFAULT_NUM_BUCKETS)
      : SharedMemoryMap<K, V>(shared_memory_name, internal_map_name) {
      initialize(buckets);
    }

  void reserve(std::size_t buckets) {
    internal_map->reserve(buckets);
  }

  std::size_t count(K &key) {
    return internal_map->count(key);
  }

  protected:
    // Boost unordered_map that contains the actual data.
    typedef std::pair<const K, V> MapValueType;
    typedef boost::interprocess::allocator<
        MapValueType, managed_shared_memory::segment_manager> MapValueAllocator;
    typedef boost::unordered_map<K, V, boost::hash<K>, std::equal_to<K>,
        MapValueAllocator> InternalMap;
    InternalMap *internal_map;

  private:

    void initialize(std::size_t buckets) {
      // TODO: Track the number of processes that have mapped this object.
      managed_shared_memory *shm = SharedMemoryMap<K,V>::shm;
      MapValueAllocator alloc_inst(shm->get_segment_manager());
      boost::interprocess::string data_key = SharedMemoryMap<K, V>::data_key;
      internal_map = shm->find_or_construct<InternalMap>(
          data_key.c_str())(buckets,
                            boost::hash<K>(),
                            std::equal_to<K>(),
                            shm->get_allocator<MapValueType>());
    }
};

}  // namespace shared
}  // namespace xiosim

#endif
