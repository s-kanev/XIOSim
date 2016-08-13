#ifndef _SIZE_CLASS_CACHE_H_
#define _SIZE_CLASS_CACHE_H_

#include <algorithm>
#include <cassert>
#include <iostream>
#include <list>
#include <map>
#include <memory>

#include "host.h"
#include "stat_database.h"
#include "stats.h"

#ifndef SIZE_CACHE_ASSERT
#define SIZE_CACHE_ASSERT(cond) assert((cond))
#endif

//#define SIZE_CLASS_CACHE_DEBUG

// Stores the size class, the allocated size it maps to, and the head of its
// free list.
class cache_entry_t {
  public:
    cache_entry_t(size_t cl, size_t _size, void* _head, void* _next)
        : size_class(cl)
        , size(_size)
        , head(_head)
        , valid_head(_head != nullptr)
        , next(_next)
        , valid_next(_next != nullptr)
        , action_id(TICK_T_MAX) {}

    cache_entry_t()
        : cache_entry_t(0, 0, nullptr, nullptr) {}

    cache_entry_t(size_t cl, size_t size)
        : cache_entry_t(cl, size, nullptr, nullptr) {}

    cache_entry_t(const cache_entry_t& pair)
        : cache_entry_t(pair.size_class, pair.size, pair.head, pair.next) {}

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

    bool has_valid_next() const { return valid_next; }
    void* get_next() const { return next; }
    void set_next(void* new_next) {
        next = new_next;
        valid_next = true;
    }
    void invalidate_next() {
        next = nullptr;
        valid_next = false;
    }

  private:
    size_t size_class;
    size_t size;
    void* head;
    bool valid_head;
    void* next;
    bool valid_next;

  public:
    seq_t action_id;
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

    static index_range_t invalid() { return index_range_t(); }
    bool operator==(const index_range_t& rhs) { return begin == rhs.begin && end == rhs.end; }
    bool operator!=(const index_range_t& rhs) { return begin != rhs.begin || end != rhs.end; }
    bool valid() { return (*this != index_range_t::invalid()); }
  private:
    size_t begin;
    size_t end;
};

std::ostream& operator<<(std::ostream& os, const index_range_t& range);
std::ostream& operator<<(std::ostream& os, const cache_entry_t& pair);

struct uop_t;  // fwd

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
    SizeClassCache(size_t size) : SizeClassCache() { set_size(size); }
    virtual ~SizeClassCache() {}

    virtual void set_size(size_t size) {
        cache_size = size;
        cache_array = std::make_unique<cache_entry_t[]>(size);
    }
    virtual void set_tid(pid_t _tid) { tid = _tid; }

    /* Searches for a mapping for the requested size.
     *
     * If the lookup hits, the size class and size pair is stored in @result and true is
     * returned. If the lookup misses, @result stores (class = 0, size = requested_size),
     * and false is returned.
     */
    virtual bool size_lookup(size_t requested_size, cache_entry_t& result) {
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
        SIZE_CACHE_ASSERT(lru.size() == cache.size());
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
    virtual bool size_update(size_t orig_size, size_t size, size_t cl) {
        size_t orig_cl_idx = compute_index(orig_size);
        size_t new_cl_idx = compute_index(size);
        index_range_t range(orig_cl_idx, new_cl_idx + 1);
#ifdef SIZE_CLASS_CACHE_DEBUG
        cache_entry_t current(cl, size);
        std::cerr << "[Size class cache]: Inserting mapping: index range = " << range
                  << ", entry = " << current;
#endif
        index_range_t current_range = find_index_range(cl);
        if (current_range.valid()) {
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
        SIZE_CACHE_ASSERT(lru.size() == cache.size());
        return true;
    }

    /* Get the next head and the next next pointer for a size class.
     *
     * If found, stores the next head pointer in @next_head, the second next in @next_next
     * and returns true.
     * If *either of them* is invalid, stores nullptr in both and returns false.
     * (we'll probably relax this one very soon).
     */
    virtual bool head_pop(size_t size_class, void** next_head, void** next_next) {
        trace_pop_start(size_class);

        index_range_t range = find_index_range(size_class);
        /* Index not in the cache, or both items missing. */
        if (!range.valid() ||
            (!get_cache_entry(range).has_valid_head() && !get_cache_entry(range).has_valid_next())) {
            *next_head = nullptr;
            *next_next = nullptr;

            head_misses++;
            next_misses++;

            trace_pop_end(false, *next_head, *next_next);
            return false;
        }

        cache_entry_t& entry = get_cache_entry(range);
        /* We have an index with a valid head and next. */
        if (entry.has_valid_head() && entry.has_valid_next()) {
            *next_head = entry.get_head();
            *next_next = entry.get_next();

            /* Move next to head if next was kosher. */
            entry.set_head(*next_next);
            if (*next_next == nullptr)
                entry.invalidate_head();

            /* and next is no more */
            entry.invalidate_next();
            SIZE_CACHE_ASSERT(lru.size() == cache.size());

            head_hits++;
            next_hits++;

            trace_pop_end(true, *next_head, *next_next);
            return true;
        }

        /* We have a head, but no next. Still a miss, but invalidate head,
         * because we'll pop it in the SW path. */
        if (entry.has_valid_head() && !entry.has_valid_next()) {
            *next_head = entry.get_head();
            *next_next = nullptr;

            entry.invalidate_head();
            SIZE_CACHE_ASSERT(lru.size() == cache.size());

            head_hits++;
            next_misses++;

            trace_pop_end(false, *next_head, *next_next);
            return false;

        }
        trace_pop_end(false, nullptr, entry.get_next());
        SIZE_CACHE_ASSERT(false && "Entry with invalid head and valid next.");
        return true;
    }

    /* Push a head pointer for a size class (shifit it right).
     *
     * If the size class exists in the cache and:
     *    - new_head == NULL, both head and next are invalidated.
     *    - new_head != NULL, the entry, updates the head and makes next the old head.
     *
     * and true returned. Otherwise, false is returned.
     */
    virtual bool head_push(size_t size_class, void* new_head) {
        index_range_t range = find_index_range(size_class);
        bool success = false;
        if (!range.valid()) {
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
            cache_entry_t& entry = get_cache_entry(range);
            success = true;
            if (new_head == nullptr) {
                entry.invalidate_head();
                head_invalidates++;
                entry.invalidate_next();
                next_invalidates++;
            } else {
                void* old_head = entry.get_head();
                entry.set_next(old_head);
                if (!old_head) { // pushing to an empty list
                    entry.invalidate_next();
                    next_invalidates++;
                } else
                    next_updates++;
                entry.set_head(new_head);
                head_updates++;
            }
        }

        trace_head_push(success, size_class, new_head);
        SIZE_CACHE_ASSERT(lru.size() == cache.size());
        return success;
    }

    /* Prefetch a next pointer in the right side of an entry.
     *
     * Two invariants:
     *   - we only prefetch to an entry with a valid head (this is a LL after all).
     *   - we only prefetch if the right slot was invalid (or the list has been compromised)
     * Returns false if no entry or no head.
     */
    virtual bool prefetch_next(size_t size_class, void* new_next) {
        trace_prefetch_next_start(size_class, new_next);

        index_range_t range = find_index_range(size_class);
        if (!range.valid()) {
            trace_prefetch_next_end(false);
            return false;
        }

        cache_entry_t& entry = get_cache_entry(range);
        bool has_head = entry.has_valid_head();
        bool has_next = entry.has_valid_next();
        bool is_invalidate = (new_next == nullptr);

        /* No head or next, prefetch straight to head. */
        if (!has_head && !has_next) {
            if (!is_invalidate) {
                head_updates++;
                entry.set_head(new_next);
            }

            trace_prefetch_next_end(true);
            SIZE_CACHE_ASSERT(lru.size() == cache.size());
            return true;
        }

        /* Both head and next, only invalidates are ok. */
        if (has_head && has_next) {
            SIZE_CACHE_ASSERT(is_invalidate && "Tried to update non-null next.");
            entry.invalidate_next();
            next_invalidates++;

            trace_prefetch_next_end(true);
            SIZE_CACHE_ASSERT(lru.size() == cache.size());
            return true;
        }

        /* Only head, prefetch in next slot. Or invalidate head. */
        if (has_head && !has_next) {
            if (!is_invalidate) {
                entry.set_next(new_next);
                next_updates++;
            } else {
                entry.invalidate_head();
                head_invalidates++;
            }
            trace_prefetch_next_end(true);
            SIZE_CACHE_ASSERT(lru.size() == cache.size());
            return true;
        }

        SIZE_CACHE_ASSERT(false && "LL with next and no head.");

        trace_prefetch_next_end(false);
        SIZE_CACHE_ASSERT(lru.size() == cache.size());
        return false;
    }

    void invalidate_entry(size_t size_class) {
        trace_invalidate(size_class);

        index_range_t range = find_index_range(size_class);
        if (range.valid())
            return;

        cache_entry_t& entry = get_cache_entry(range);
        entry.invalidate_head();
        head_invalidates++;
        entry.invalidate_next();
        next_invalidates++;
    }

    // Flushes the cache.
    virtual void flush() {
#ifdef SIZE_CLASS_CACHE_DEBUG
        std::cerr << "Flushing size class cache." << std::endl;
#endif
        cache.clear();
        lru.clear();
        SIZE_CACHE_ASSERT(lru.size() == cache.size());
    }

    virtual void print() {
        for (auto it = cache.begin(); it != cache.end(); it++) {
            std::cerr << it->first << "\t" << cache_array[it->second] << std::endl;
        }
    }

  protected:
    bool get_cache_entry(index_range_t key, cache_entry_t& result) {
        if (cache.find(key) == cache.end())
            return false;
        size_t result_ind = cache[key];
        result = cache_array[result_ind];
        update_lru(result.get_size_class());
        return true;
    }

    index_range_t find_index_range(size_t cl, bool touch_lru = true) {
        for (auto& cache_pair : cache) {
            if (cache_array[cache_pair.second].get_size_class() == cl) {
                if (touch_lru)
                    update_lru(cl);
                return cache_pair.first;
            }
        }
        return index_range_t::invalid();
    }

    cache_entry_t& get_cache_entry(index_range_t key, bool touch_lru = true) {
        SIZE_CACHE_ASSERT(cache.find(key) != cache.end());
        size_t result_ind = cache[key];
        auto& result = cache_array[result_ind];
        if (touch_lru)
            update_lru(result.get_size_class());
        return result;
    }

    void insert_cache_entry(index_range_t key, cache_entry_t entry) {
        SIZE_CACHE_ASSERT(cache.size() < cache_size && cache.find(key) == cache.end());
        size_t i;
        for (i = 0; i < cache_size; i++) {
            auto found = std::find_if(cache.begin(), cache.end(), [i](auto& x) { return x.second == i; });
            if (found == cache.end())
                break;
        }
        cache[key] = i;
        cache_array[i] = entry;
        update_lru(entry.get_size_class());
    }

    void update_cache_key(index_range_t old_key, index_range_t new_key) {
        SIZE_CACHE_ASSERT(cache.find(old_key) != cache.end());
        size_t current_ind = cache[old_key];
        cache.erase(old_key);
        cache[new_key] = current_ind;
    }

    cache_entry_t* get_line_ptr(index_range_t key) {
        size_t ind = cache[key];
        return cache_array.get() + ind;
    }

    /* Update the LRU chains.
     *
     * If inserting a new element into the cache when the cache was full,
     * evict() must have already been called!
     */
    void update_lru(size_t cl) {
        auto it = std::find(lru.begin(), lru.end(), cl);
        if (it == lru.end()) {
            SIZE_CACHE_ASSERT(lru.size() < cache_size && "Attempted to add to LRU while LRU chain was full!");
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
        index_range_t range = find_index_range(evicted_cl);
        SIZE_CACHE_ASSERT(range.valid() && "Element to evict does not exist in the cache!");
#ifdef SIZE_CLASS_CACHE_DEBUG
        cache_entry_t evicted_entry = get_cache_entry(range);
        std::cerr << "[Size class cache]: Evicting index range = " << range
                  << ", entry = " << evicted_entry << std::endl;
#endif
        cache_array[cache[range]].action_id = TICK_T_MAX;
        cache.erase(range);
        lru.pop_front();  // find_index_range calls update_lru(cl), which moves cl to the front.
        size_evictions++;
    }

    virtual void trace_pop_start(size_t size_class) {
#ifdef SIZE_CLASS_CACHE_DEBUG
        std::cerr << "[Size class cache]: Popping head for class=" << size_class;
#endif
    }

    virtual void trace_pop_end(bool success, void* next_head, void* next_next) {
#ifdef SIZE_CLASS_CACHE_DEBUG
        if (success) {
            std::cerr << " succeeded. Returning " << next_head << " " << next_next << std::endl;
        } else
            std::cerr << " failed." << std::endl;
#endif
    }

    virtual void trace_head_push(bool success, size_t size_class, void* new_head) {
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
    }

    virtual void trace_prefetch_next_start(size_t size_class, void* new_next) {
#ifdef SIZE_CLASS_CACHE_DEBUG
        if (new_next == nullptr) {
            std::cerr << "[Size class cache]: Invalidating next for class=" << size_class;
        } else {
            std::cerr << "[Size class cache]: Updating next " << new_next
                      << " for class=" << size_class;
        }
#endif
    }

    virtual void trace_prefetch_next_end(bool success) {
#ifdef SIZE_CLASS_CACHE_DEBUG
        if (success)
            std::cerr << " succeeded." << std::endl;
        else
            std::cerr << " failed" << std::endl;
#endif
    }

    virtual void trace_invalidate(size_t size_class) {
#ifdef SIZE_CLASS_CACHE_DEBUG
        std::cerr << "[Size class cache]: Invalidating head & next for class=" << size_class << std::endl;
#endif
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
    uint64_t next_hits;
    uint64_t next_misses;
    uint64_t next_updates;
    uint64_t next_invalidates;

    // Constants from tcmalloc.
    const size_t kMaxSmallSize = 1024;
    const size_t kMaxSize = 256 * 1024;

    // Size of the cache.
    size_t cache_size;

    // Id of the owning thread.
    pid_t tid;

    // Maps a size class index range to an index in the array.
    std::map<index_range_t, size_t> cache;
    // The actual (fixed) storage array (size class, allocated size, and next head ptr).
    std::unique_ptr<cache_entry_t[]> cache_array;

    // LRU chain where each element is a size class and the least
    // recently used item is the last element.
    std::list<size_t> lru;
};
#endif
