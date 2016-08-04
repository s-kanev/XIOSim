#ifndef _SIZE_CLASS_CACHE_H_
#define _SIZE_CLASS_CACHE_H_

#include <algorithm>
#include <cassert>
#include <iostream>
#include <list>
#include <map>

#include "stat_database.h"
#include "stats.h"

// Stores the size class, the allocated size it maps to, and the head of its
// free list.
class cache_entry_t {
  public:
    cache_entry_t()
        : size_class(0)
        , size(0)
        , valid_head(false)
        , head(nullptr) {}

    cache_entry_t(size_t cl, size_t size)
        : size_class(cl)
        , size(size)
        , valid_head(false)
        , head(nullptr) {}

    cache_entry_t(size_t cl, size_t size, void* _head)
        : size_class(cl)
        , size(size)
        , head(_head) {
        valid_head = (head != nullptr);
    }

    cache_entry_t(const cache_entry_t& pair) : cache_entry_t(pair.size_class, pair.size) {
        valid_head = pair.valid_head;
        head = pair.head;
    }

    // For equality, don't check the head pointer since it will often change.
    bool operator==(const cache_entry_t& pair) {
        return size_class == pair.size_class && size == pair.size;
    }

    size_t get_size_class() const { return size_class; }
    size_t get_size() const { return size; }
    bool has_valid_head() const { return valid_head; }
    void* get_head() const { return head; }
    void set_head(void* new_head) {
        head = new_head;
        valid_head = true;
    }
    void invalidate_head() {
        head = nullptr;
        valid_head = false;
    }

  private:
    size_t size_class;
    size_t size;
    bool valid_head;
    void* head;
};

/* Maps a range of size class indices to a size class pair.
 *
 * The range is defined as integers in [begin, end), which can be expanded through expand().
 */
class index_range_t {
  public:
    index_range_t()
        : begin(0)
        , end(0) {}

    index_range_t(size_t _begin, size_t _end)
        : begin(_begin)
        , end(_end) {}

    bool contains(size_t index) { return (index >= begin && index < end); }
    void expand(size_t value) {
        if (value < begin)
            begin = value;
        else if (value > end)
            end = value + 1;
    }

    /* Returns true if the bounds of this index range are both less than the
     * beginning of the other index range.
     */
    bool operator<(const index_range_t& other) const {
        return begin < other.begin && end <= other.begin;
    }
    size_t get_begin() const { return begin; }
    size_t get_end() const { return end; }

  private:
    size_t begin;
    size_t end;
};

std::ostream& operator<<(std::ostream& os, const index_range_t& range);
std::ostream& operator<<(std::ostream& os, const cache_entry_t& pair);

/* A cache for mappings between size classes and the allocated size.
 *
 * The cache is indexed by a range of size class indices (see compute_index()), as
 * multiple size class indices can map to the same size class. Ranges are
 * updated dynamically on misses. Evictions are based on an LRU policy.
 *
 * Each thread carries its own state, and on thread switches the cache is flushed.
 */
class SizeClassCache {
  public:
    SizeClassCache()
        : size_hits(0)
        , size_misses(0)
        , size_insertions(0)
        , size_evictions(0)
        , head_hits(0)
        , head_misses(0)
        , head_updates(0)
        , head_invalidates(0)
        , cache_size(0) {}
    SizeClassCache(size_t size) : SizeClassCache() { cache_size = size; }
    ~SizeClassCache() {}

    void set_size(size_t size) { cache_size = size; }
    void set_tid(pid_t _tid) { tid = _tid; }

    /* Searches for a mapping for the requested size.
     *
     * If the lookup hits, the size class and size pair is stored in @result and true is
     * returned. If the lookup misses, @result stores (class = 0, size = requested_size),
     * and false is returned.
     */
    bool size_lookup(size_t requested_size, cache_entry_t& result) {
        size_t cl_index = compute_index(requested_size);
        index_range_t key(cl_index, cl_index+1);
        bool hit = get_cache_entry(key, result);
        if (hit) {
            size_hits++;
#ifdef SIZE_CLASS_CACHE_DEBUG
            std::cerr << "[Size class cache]: Found entry for size " << requested_size
                      << ": index range = " << key << ", entry = " << result << std::endl;
#endif
        } else {
#ifdef SIZE_CLASS_CACHE_DEBUG
            std::cerr << "[Size class cache]: Failed to find entry for size " << requested_size
                      << " with index " << cl_index << std::endl;
#endif
            size_misses++;
            result = cache_entry_t(0, requested_size);
        }
        assert(lru.size() == cache.size());
        return hit;
    }

    /* Update the cache with a new size class and size mapping.
     *
     * The mapping will be for a range from the index of @orig_size to the index
     * of @size. orig_size must be provided for ranges to expand correctly.
     *
     * Arguments:
     *    orig_size: the original requested size.
     *    size: the allocated size.
     *    cl: allocated size class.
     *
     * Returns:
     *    true if a new entry was created or an existing entry was expanded, false otherwise.
     */
    bool size_update(size_t orig_size, size_t size, size_t cl) {
        size_t orig_cl_idx = compute_index(orig_size);
        size_t new_cl_idx = compute_index(size);
        index_range_t range(orig_cl_idx, new_cl_idx + 1);
#ifdef SIZE_CLASS_CACHE_DEBUG
        cache_entry_t current(cl, size);
        std::cerr << "[Size class cache]: Inserting mapping: index range = " << range
                  << ", entry = " << current;
#endif
        auto it = find_index_range(cl);
        if (it != cache.end()) {
            index_range_t current_range = it->first;
            if (current_range.contains(orig_cl_idx)) {
#ifdef SIZE_CLASS_CACHE_DEBUG
                std::cerr << " did nothing: already present." << std::endl;
#endif
                return false;
            } else {
                // Found the size class pair but the index was out of the existing
                // range, so expand the index range and re-add it to the cache.
                update_cache_key(current_range, range);
#ifdef SIZE_CLASS_CACHE_DEBUG
                std::cerr << " succeeded: index range expanded." << std::endl;
#endif
            }
        } else {
            // Did not find this size class pair, so insert a new mapping.
#ifdef SIZE_CLASS_CACHE_DEBUG
            std::cerr << " succeeded: created new mapping." << std::endl;
#endif
            if (cache.size() == cache_size)
                evict();
            insert_cache_entry(range, cache_entry_t(cl, size));
        }
        assert(lru.size() == cache.size());
        return true;
    }

    /* Get the next head pointer for a size class.
     *
     * If found, stores the next head pointer in @next_head and returns true.
     * Otherwise, stores nullptr in @next_head and returns false.
     */
    bool head_pop(size_t size_class, void** next_head) {
        auto it = find_index_range(size_class);
        bool success = false;
        if (it != cache.end() && it->second.has_valid_head()) {
            *next_head = it->second.get_head();
            it->second.invalidate_head();
            head_hits++;
            success = true;
        } else {
            *next_head = nullptr;
            head_misses++;
            success = false;
        }
        assert(lru.size() == cache.size());
#ifdef SIZE_CLASS_CACHE_DEBUG
        std::cerr << "Popping head for class=" << size_class;
        if (success)
            std::cerr << " succeeded. Returning " << *next_head << std::endl;
        else
            std::cerr << " failed." << std::endl;
#endif
        return success;
    }

    /* Updates a head pointer for a size class.
     *
     * If the size class exists in the cache and:
     *    - new_head == NULL, the entry is invalidated.
     *    - new_head != NULL, the entry is updated.
     *
     * and true returned. Otherwise, false is returned.
     */
    bool head_update(size_t size_class, void* new_head) {
        auto it = find_index_range(size_class);
        bool success = false;
        if (it == cache.end()) {
#ifdef SIZE_CLASS_CACHE_SLL_ONLY
            // Insert this head pointer, evicting another entry if needed.
            if (cache.size() == cache_size)
                evict();
            // We don't get size information, so we can't compute the actual index.
            index_range_t fake_index(size_class, size_class + 1);
            cache_entry_t new_entry(size_class, 0, new_head);
            insert_cache_entry(fake_index, new_entry);
            head_updates++;
            success = true;
#else
            success = false;
#endif
        } else {
          success = true;
          if (new_head == nullptr) {
              it->second.invalidate_head();
              head_invalidates++;
          } else {
              it->second.set_head(new_head);
              head_updates++;
          }
        }
        assert(lru.size() == cache.size());
#ifdef SIZE_CLASS_CACHE_DEBUG
        if (new_head == nullptr) {
            std::cerr << "Invalidating head for class=" << size_class;
        } else {
          std::cerr << "[Size class cache]: Updating head " << new_head
                    << " for class=" << size_class;
        }
        if (success)
            std::cerr << " succeeded." << std::endl;
        else
            std::cerr << " failed" << std::endl;
#endif
        return success;
    }

    // Flushes the cache.
    void flush() {
#ifdef SIZE_CLASS_CACHE_DEBUG
        std::cerr << "Flushing size class cache." << std::endl;
#endif
        cache.clear();
        lru.clear();
        assert(lru.size() == cache.size());
    }

    void reg_stats(xiosim::stats::StatsDatabase* sdb, int coreID) {
        using namespace xiosim::stats;

        auto& size_hits_stat =
                stat_reg_core_qword(sdb, true, coreID, "size_class_cache.size_hits",
                                    "Size class hits", &size_hits, 0, true, NULL);
        auto& size_misses_stat =
                stat_reg_core_qword(sdb, true, coreID, "size_class_cache.size_misses",
                                    "Size class misses", &size_misses, 0, true, NULL);
        stat_reg_core_qword(sdb, true, coreID, "size_class_cache.size_insertions",
                            "Size class insertions", &size_insertions, 0, true, NULL);
        stat_reg_core_qword(sdb, true, coreID, "size_class_cache.size_evictions",
                            "Size class evictions", &size_evictions, 0, true, NULL);
        stat_reg_core_formula(sdb, true, coreID, "size_class_cache.size_hit_rate",
                              "Size class hit rate (size class)",
                              size_hits_stat / (size_hits_stat + size_misses_stat), "%12.8f");

        auto& head_hits_stat =
                stat_reg_core_qword(sdb, true, coreID, "size_class_cache.head_hits",
                                    "Head pointer hits", &head_hits, 0, true, NULL);
        auto& head_misses_stat =
                stat_reg_core_qword(sdb, true, coreID, "size_class_cache.head_misses",
                                    "Head pointer misses", &head_misses, 0, true, NULL);
        stat_reg_core_qword(sdb, true, coreID, "size_class_cache.head_updates",
                            "Head pointer insertions", &head_updates, 0, true, NULL);
        stat_reg_core_qword(sdb, true, coreID, "size_class_cache.head_invalidates",
                            "Head pointer invalidates", &head_invalidates, 0, true, NULL);
        stat_reg_core_formula(sdb, true, coreID, "size_class_cache.head_hit_rate",
                              "Head pointer hit rate (head)",
                              head_hits_stat / (head_hits_stat + head_misses_stat), "%12.8f");

        stat_reg_core_formula(
                sdb, true, coreID, "size_class_cache.accesses", "Size class total accesses",
                size_hits_stat + size_misses_stat + head_hits_stat + head_misses_stat, "%12.0f");
    }

  private:
    std::map<index_range_t, cache_entry_t>::iterator find_index_range(size_t cl) {
        for (auto it = cache.begin(); it != cache.end(); ++it) {
            if (it->second.get_size_class() == cl) {
                update_lru(cl);
                return it;
            }
        }
        return cache.end();
    }

    bool get_cache_entry(index_range_t key, cache_entry_t& result) {
        if (cache.find(key) == cache.end())
            return false;
        result = cache[key];
        update_lru(result.get_size_class());
        return true;
    }

    void insert_cache_entry(index_range_t key, cache_entry_t entry) {
        assert(cache.size() < cache_size && cache.find(key) == cache.end());
        cache[key] = entry;
        update_lru(entry.get_size_class());
    }

    void update_cache_key(index_range_t old_key, index_range_t new_key) {
        assert(cache.find(old_key) != cache.end());
        cache_entry_t current_entry = cache[old_key];
        cache.erase(old_key);
        cache[new_key] = current_entry;
    }

    /* Update the LRU chains.
     *
     * If inserting a new element into the cache when the cache was full,
     * evict() must have already been called!
     */
    void update_lru(size_t cl) {
        auto it = std::find(lru.begin(), lru.end(), cl);
        if (it == lru.end()) {
            assert(lru.size() < cache_size && "Attempted to add to LRU while LRU chain was full!");
            // This is a new element. Add it to the LRU chain.
            lru.push_front(cl);
        } else {
            // This is an existing element. Move it to the front.
            size_t idx = *it;
            lru.erase(it);
            lru.push_front(idx);
        }
    }

    // Compute the size class index.
    size_t compute_index(size_t size) {
        if (size <= kMaxSmallSize) {
            return (static_cast<uint32_t>(size) + 7) >> 3;
        } else {
            return (static_cast<uint32_t>(size) + 127 + (120 << 7)) >> 7;
        }
    }

    // Evict the least recently used element.
    void evict() {
        size_t evicted_cl = lru.back();
        auto it = find_index_range(evicted_cl);
        assert(it != cache.end() && "Element to evict does not exist in the cache!");
#ifdef SIZE_CLASS_CACHE_DEBUG
        cache_entry_t evicted_entry = it->second;
        std::cerr << "[Size class cache]: Evicting index range = " << it->first
                  << ", entry = " << evicted_entry << std::endl;
#endif
        cache.erase(it);
        lru.pop_front();  // find_index_range calls update_lru(cl), which moves cl to the front.
        size_evictions++;
    }

    void print() {
        for (auto it = cache.begin(); it != cache.end(); it++) {
            std::cerr << it->first << "\t" << it->second << std::endl;
        }
    }

    // Statistics.
    uint64_t size_hits;
    uint64_t size_misses;
    uint64_t size_insertions;
    uint64_t size_evictions;
    uint64_t head_hits;
    uint64_t head_misses;
    uint64_t head_updates;
    uint64_t head_invalidates;

    // Constants from tcmalloc.
    const size_t kMaxSmallSize = 1024;
    const size_t kMaxSize = 256 * 1024;

    // Size of the cache.
    size_t cache_size;

    // Id of the owning thread.
    pid_t tid;

    // Maps a size class index range to the size class, allocated size, and next head ptr.
    std::map<index_range_t, cache_entry_t> cache;

    // LRU chain where each element is a size class and the least
    // recently used item is the last element.
    std::list<size_t> lru;
};

#endif
