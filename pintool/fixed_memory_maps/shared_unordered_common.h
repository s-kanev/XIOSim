// Parent class of the unordered map wrapper that contains all functionality
// that is type-independent.
//
// Author: Sam Xi

#ifndef SHARED_UNORDERED_MAP_COMMON_H
#define SHARED_UNORDERED_MAP_COMMON_H

#include <boost/functional/hash.hpp>  // boost::hash
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/unordered_map.hpp>

#include <cstdlib>  // for size_t
#include <functional>  // for std::equal_to
#include <utility>  // for std::pair
#include <iostream>

#include "mpkeys.h"

namespace xiosim {
namespace shared {

using namespace boost::interprocess;

template<typename K, typename V>
class SharedUnorderedMapCommon {
  protected:
    static const std::size_t DEFAULT_NUM_BUCKETS = 16;
    typedef std::pair<const K, V> MapValueType;
    typedef boost::interprocess::allocator<
        MapValueType, fixed_managed_shared_memory::segment_manager>
          MapValueAllocator;
    typedef boost::interprocess::allocator<void, fixed_managed_shared_memory::segment_manager>
        VoidAllocator;
    typedef boost::unordered_map<K, V, boost::hash<K>, std::equal_to<K>,
        MapValueAllocator> InternalMap;

  public:
    SharedUnorderedMapCommon() : data_key(""), memory_key("") {
    }

    SharedUnorderedMapCommon(
        const char* shared_memory_name, const char* internal_map_name,
        std::size_t buckets = DEFAULT_NUM_BUCKETS, void* addr=0x0)
        : data_key(internal_map_name), memory_key(shared_memory_name) {
      shm = new fixed_managed_shared_memory(open_or_create, shared_memory_name,
          DEFAULT_SHARED_MEMORY_SIZE, addr);
      initialize(buckets);
    }

    void initialize_late(
        const char* shared_memory_name, const char* internal_map_name,
        std::size_t buckets = DEFAULT_NUM_BUCKETS, void* addr=0x0) {
      memory_key = boost::interprocess::string(shared_memory_name);
      data_key = boost::interprocess::string(internal_map_name);
      shm = new fixed_managed_shared_memory(
          open_or_create, memory_key.c_str(), DEFAULT_SHARED_MEMORY_SIZE, addr);
      initialize(buckets);
    }

    // Returns a reference to the value stored at key. If such a key does
    // not exist, a value is default-constructed, unserted into the map, and the
    // reference to the new pair is returned.
    // This emulates the semantics of the [] operator of a map, because there
    // are some strange errors that prevent simply calling operator[]() on the
    // underlying map.
    // Returns a reference to the value stored at key. If the key-value
    // pair does not exist, an out-of-range exception is thrown.
    V& operator[](K key) {
      return access_operator(key);
    }

    void insert(K key, V value) {
      MapValueType new_pair(key, value);
      try {
        MapValueAllocator alloc_inst(shm->get_segment_manager());
        internal_map->insert(new_pair);
      } catch (const boost::interprocess::bad_alloc&) {
        assert(false);
      }
    }

    V& at(const K key) {
      return internal_map->at(key);
    }

    std::size_t size() {
      return internal_map->size();
    }

    typename InternalMap::iterator begin() {
      return internal_map->begin();
    }

    typename InternalMap::iterator end() {
      return internal_map->end();
    }

    void reserve(std::size_t buckets) {
      internal_map->reserve(buckets);
    }

    std::size_t count(K &key) {
      return internal_map->count(key);
    }

  protected:
    // Name that identifies this map in the shared memory segment.
    boost::interprocess::string data_key;
    // Name that identifies this shared memory segment.
    boost::interprocess::string memory_key;
    // Shared memory segment. This must always be mapped into memory in order to
    // maintain consistency.
    fixed_managed_shared_memory *shm;
    // Boost unordered_map that contains the actual data.
    InternalMap *internal_map;

  private:
    virtual V& access_operator(const K& key) = 0;

    void initialize(std::size_t buckets) {
      // TODO: Track the number of processes that have mapped this object.
      MapValueAllocator alloc_inst(shm->get_segment_manager());
      internal_map = shm->template find_or_construct<InternalMap>(data_key.c_str())
        (buckets, boost::hash<K>(), std::equal_to<K>(), alloc_inst);
    }
};

}  // namespace shared
}  // namespace xiosim

#endif
