#ifndef _SIZE_CLASS_CACHE_H_
#define _SIZE_CLASS_CACHE_H_

#include <algorithm>
#include <cassert>
#include <iostream>
#include <list>
#include <map>

#include "stat_database.h"
#include "stats.h"

// Stores a pair of size class and the allocated size it maps to.
class size_class_pair_t {
  public:
    size_class_pair_t()
        : size_class(0)
        , size(0) {}

    size_class_pair_t(size_t cl, size_t size)
        : size_class(cl)
        , size(size) {}

    size_class_pair_t(const size_class_pair_t& pair)
        : size_class_pair_t(pair.size_class, pair.size) {}

    bool operator==(const size_class_pair_t& pair) {
        return size_class == pair.size_class && size == pair.size;
    }

    size_t get_size_class() const { return size_class; }
    size_t get_size() const { return size; }

  private:
    size_t size_class;
    size_t size;

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

    bool operator<(const index_range_t& other) const { return begin < other.begin; }
    size_t get_begin() const { return begin; }
    size_t get_end() const { return end; }

  private:
    size_t begin;
    size_t end;
};

std::ostream& operator<<(std::ostream& os, const index_range_t& range);
std::ostream& operator<<(std::ostream& os, const size_class_pair_t& pair);

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
        : hits(0)
        , misses(0)
        , insertions(0)
        , evictions(0)
        , cache_size(0) {}
    SizeClassCache(size_t size)
        : hits(0)
        , misses(0)
        , insertions(0)
        , evictions(0)
        , cache_size(size) {}
    ~SizeClassCache() {}

    void set_size(size_t size) { cache_size = size; }
    void set_tid(pid_t _tid) { tid = _tid; }

    bool lookup(size_t requested_size, size_class_pair_t& result) {
        size_t cl_index = compute_index(requested_size);
        index_range_t key(cl_index, cl_index+1);
        if (cache.find(key) == cache.end()) {
#ifdef SIZE_CLASS_CACHE_DEBUG
            std::cerr << "[Size class cache]: Failed to find entry for size " << requested_size
                      << " with index " << cl_index << std::endl;
#endif
            misses++;
            return false;
        } else {
            result = cache[key];
            update_lru(result.get_size_class());
            hits++;
#ifdef SIZE_CLASS_CACHE_DEBUG
            std::cerr << "[Size class cache]: Found entry for size " << requested_size
                      << ": index range = " << key << ", entry = " << result << std::endl;
#endif
            return true;
        }
    }

    void update(size_t orig_size, size_t size, size_t cl) {
        size_t orig_cl_idx = compute_index(orig_size);
        size_t new_cl_idx = compute_index(size);
        size_class_pair_t current(cl, size);
        index_range_t range(orig_cl_idx, new_cl_idx + 1);
#ifdef SIZE_CLASS_CACHE_DEBUG
        std::cerr << "[Size class cache]: Inserting mapping: index range = " << range
                  << ", entry = " << current;
#endif
        auto it = find_index_range(cl);
        if (it != cache.end()) {
            index_range_t current_range = it->first;
            if (current_range.contains(new_cl_idx)) {
#ifdef SIZE_CLASS_CACHE_DEBUG
                std::cerr << " did nothing: already present." << std::endl;
#endif
            } else {
                // Found the size class pair but the index was out of the existing
                // range, so expand the index range and re-add it to the cache.
                cache.erase(current_range);
                current_range.expand(new_cl_idx);
                cache[current_range] = current;
#ifdef SIZE_CLASS_CACHE_DEBUG
                std::cerr << " succeeded: index range expanded." << std::endl;
#endif
            }
        } else {
            // Did not find this size class pair, so insert a new mapping.
            if (cache.size() == cache_size)
                evict();
            cache[range] = current;
#ifdef SIZE_CLASS_CACHE_DEBUG
            std::cerr << " succeeded: created new mapping." << std::endl;
#endif
        }
        update_lru(cl);
    }

    // Flushes the cache.
    void flush() {
        cache.clear();
        lru.clear();
    }

    void reg_stats(xiosim::stats::StatsDatabase* sdb, int coreID) {
        using namespace xiosim::stats;
        const size_t LEN = 64;
        char stat_name_prefix[LEN];
        char hits_name[LEN];
        char misses_name[LEN];
        char evictions_name[LEN];
        char insertions_name[LEN];
        char accesses_name[LEN];
        char hit_rate_name[LEN];

        snprintf(stat_name_prefix, LEN, "size_class_cache.%d.", tid);
        snprintf(hits_name, LEN, "%s.%s.", stat_name_prefix, "hits");
        snprintf(misses_name, LEN, "%s.%s.", stat_name_prefix, "misses");
        snprintf(insertions_name, LEN, "%s.%s.", stat_name_prefix, "insertions");
        snprintf(evictions_name, LEN, "%s.%s.", stat_name_prefix, "evictions");
        snprintf(accesses_name, LEN, "%s.%s.", stat_name_prefix, "accesses");
        snprintf(hit_rate_name, LEN, "%s.%s.", stat_name_prefix, "hit_rate");

        auto& hits_stat = stat_reg_core_qword(sdb, true, coreID, hits_name,
                                              "Size class hits", &hits, 0, true, NULL);
        auto& misses_stat = stat_reg_core_qword(sdb, true, coreID, misses_name,
                                                "Size class misses", &misses, 0, true, NULL);
        stat_reg_core_qword(sdb, true, coreID, insertions_name,
                            "Size class insertions", &insertions, 0, true, NULL);
        stat_reg_core_qword(sdb, true, coreID, evictions_name, "Size class evictions",
                            &evictions, 0, true, NULL);
        stat_reg_core_formula(sdb, true, coreID, accesses_name,
                              "Size class total accesses", hits_stat + misses_stat, NULL);
        stat_reg_core_formula(sdb, true, coreID, hit_rate_name, "Size class hit rate",
                              hits_stat / (hits_stat + misses_stat), NULL);
    }

  private:
    std::map<index_range_t, size_class_pair_t>::iterator find_index_range(size_t cl) {
        for (auto it = cache.begin(); it != cache.end(); ++it)
            if (it->second.get_size_class() == cl)
                return it;
        return cache.end();
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
        size_class_pair_t evicted_entry = it->second;
        std::cerr << "[Size class cache]: Evicting index range = " << it->first
                  << ", entry = " << evicted_entry << std::endl;
#endif
        cache.erase(it);
        lru.pop_back();
        evictions++;
    }

    // Statistics.
    uint64_t hits;
    uint64_t misses;
    uint64_t insertions;
    uint64_t evictions;

    // Constants from tcmalloc.
    const size_t kMaxSmallSize = 1024;
    const size_t kMaxSize = 256 * 1024;

    // Size of the cache.
    size_t cache_size;

    // Id of the owning thread.
    pid_t tid;

    // Maps a size class index range to the size class and the allocated size.
    std::map<index_range_t, size_class_pair_t> cache;

    // LRU chain where each element is a size class and the least
    // recently used item is the last element.
    std::list<size_t> lru;
};

#endif
