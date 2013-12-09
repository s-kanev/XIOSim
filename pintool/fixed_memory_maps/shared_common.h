// Parent class for all shared memory maps. Contains all the common code for
// performing map operations. Children must implement the private
// access_operator() function.
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
#include "mpkeys.h"

namespace xiosim {
namespace shared {

using namespace boost::interprocess;

// When the shared memory segment's free space is insufficient, upon the next
// insert/update operation, expand it by this many bytes.
const std::size_t DEFAULT_GROW_MEMORY_SIZE = 65536;

template<typename K, typename V>
class SharedMemoryMapCommon {
  protected:
    typedef fixed_managed_shared_memory::segment_manager segment_manager_t;
    typedef std::pair<std::size_t, std::size_t> MemorySizeStateType;
    typedef std::pair<const K, V> MapValueType;
    typedef boost::interprocess::allocator<
        MapValueType, segment_manager_t>
          MapValueAllocator;
    typedef boost::interprocess::map<K, V, std::less<K>, MapValueAllocator>
        InternalMap;
    typedef boost::interprocess::allocator<void, segment_manager_t>
        VoidAllocator;

  public:
    SharedMemoryMapCommon() : data_key(""), memory_key("") {}

    SharedMemoryMapCommon(
        const char* shared_memory_name, const char* internal_data_name,
        void* addr=0x0)
        : data_key(internal_data_name), memory_key(shared_memory_name) {

      shm = new fixed_managed_shared_memory(
          open_or_create, memory_key.c_str(), DEFAULT_SHARED_MEMORY_SIZE, addr);
      initialize();
    }

    // Destroys the pointer to the shared map.
    // TODO: Only delete if no processes have mapped this structure.
    ~SharedMemoryMapCommon() {}

    void initialize_late(
        const char* shared_memory_name, const char* internal_map_name,
        void* addr) {
      memory_key = boost::interprocess::string(shared_memory_name);
      data_key = boost::interprocess::string(internal_map_name);
      shm = new fixed_managed_shared_memory(
          open_or_create, memory_key.c_str(), DEFAULT_SHARED_MEMORY_SIZE, addr);
      initialize();
    }

    // Returns a reference to the value stored at key. If such a key does
    // not exist, a value is default-constructed, inserted into the map, and the
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
        check_and_grow_shared_memory();
        insert(key, value);
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

    typename InternalMap::iterator find(K key) {
      return internal_map->find(key);
    }

    std::size_t erase(const K& key) {
      return internal_map->erase(key);
    }

  protected:
    // Name that identifies this map in the shared memory segment.
    boost::interprocess::string data_key;
    // Name that identifies this shared memory segment.
    boost::interprocess::string memory_key;
    // Shared memory segment. This must always be mapped into memory in order to
    // maintain consistency.
    fixed_managed_shared_memory *shm;

    // Boost interprocess map that contains the actual data.
    InternalMap *internal_map;

  private:

    void initialize() {
      // TODO: Track the number of processes that have mapped this object.
      MapValueAllocator alloc_inst(shm->get_segment_manager());
      internal_map = shm->find_or_construct<InternalMap>(data_key.c_str())(
          std::less<K>(), alloc_inst);
    }

    virtual V& access_operator(const K& key) = 0;

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
      fixed_managed_shared_memory::grow(
          memory_key.c_str(), DEFAULT_GROW_MEMORY_SIZE);
      shm = new fixed_managed_shared_memory(open_only, memory_key.c_str());
      MemorySizeStateType new_shm_state = get_state_of_memory();
      assert(
          shm_state.first + DEFAULT_GROW_MEMORY_SIZE == new_shm_state.first);
    }
};

}  // namespace shared
}  // namespace xiosim

#endif
