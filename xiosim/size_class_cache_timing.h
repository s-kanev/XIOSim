#ifndef _SIZE_CLASS_CACHE_TIMING_H_
#define _SIZE_CLASS_CACHE_TIMING_H_

#include <algorithm>
#include <iostream>
#include <list>
#include <map>

#include "misc.h"
#include "stat_database.h"
#include "stats.h"

#include "zesto-structs.h"

#define SIZE_CACHE_ASSERT(cond) xiosim_assert((cond))
#include "size_class_cache.h"


/* SCC for when we use it on the timing side.
 * In addition to the usual things, deals with stats and supports ztrace.
 */
class SizeClassCacheReal : public SizeClassCache {
  public:
    SizeClassCacheReal(size_t size)
        : SizeClassCache(size) {}

    ~SizeClassCacheReal() {}

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

    /* Wrappers to add timing behavior to the SCC. */
    virtual bool head_pop(struct uop_t* uop, size_t size_class, void** next_head, void** next_next) {
        curr_uop = uop;  // for ztrace
        return SizeClassCache::head_pop(size_class, next_head, next_next);
    }

    virtual bool head_push(struct uop_t* uop, size_t size_class, void* new_head) {
        curr_uop = uop;  // for ztrace
        return SizeClassCache::head_push(size_class, new_head);
    }

    virtual bool prefetch_next(struct uop_t* uop, size_t size_class, void* new_next) {
        curr_uop = uop;  // for ztrace
        bool start_pf = SizeClassCache::prefetch_next(size_class, new_next);

        if (start_pf) {
            index_range_t line_range = find_index_range(size_class, false);
            xiosim_assert(line_range.valid());
            cache_entry_t* line = get_line_ptr(line_range);
            if (new_next != nullptr) {
                /* we'll wait until the pf comes back. */
                line->action_id = uop->exec.action_id;
                uop->Mop->oracle.size_class_cache.scc_entry = line;
#ifdef ZTRACE
                ztrace_print(uop, "action id in line %x SCC: %d", line, line->action_id);
#endif
            } else {
                /* invalidating, just clear action_id. */
                line->action_id = TICK_T_MAX;
            }
        }
        /* else, the pf will fail the action_id check and just fall out */

        return start_pf;
    }

    virtual void invalidate_entry(struct uop_t* uop, size_t size_class) {
        curr_uop = uop;
        SizeClassCache::invalidate_entry(size_class);
    }

    virtual bool is_ready(size_t size_class) {
        index_range_t range = find_index_range(size_class, false);
        /* entry not in cache, so can't be blocked */
        if (!range.valid())
            return true;

        /* outstanding PF, cache is blocked for this size class */
        return (get_cache_entry(range, false).action_id == TICK_T_MAX);
    }

  protected:
    int coreID;
    struct uop_t* curr_uop;

    void trace_pop_start(size_t size_class) {
#ifdef ZTRACE
        ztrace_print_start(curr_uop, "e|SCC|Popping head for class=%d", size_class);
#endif
    }

    void trace_pop_end(bool success, void* next_head, void* next_next) {
#ifdef ZTRACE
        if (success)
            ztrace_print_finish(coreID, " succeeded. Returning %x", next_head);
        else
            ztrace_print_finish(coreID, " failed.");
#endif
    }

    void trace_head_push(bool success, size_t size_class, void* new_head) {
#ifdef ZTRACE
        ztrace_print_start(curr_uop, "e|SCC|");
        if (new_head == nullptr)
            ztrace_print_cont(coreID, "Invalidating head for class=%d", size_class);
        else
            ztrace_print_cont(coreID, "Updating head %x for class=%d", new_head, size_class);

        if (success)
            ztrace_print_finish(coreID, " succeeded.");
        else
            ztrace_print_finish(coreID, " failed.");
#endif
    }

    void trace_prefetch_next_start(size_t size_class, void* new_next) {
#ifdef ZTRACE
        ztrace_print_start(curr_uop, "e|SCC|");
        if (new_next == nullptr)
            ztrace_print_cont(coreID, "Invalidating next for class=%x", size_class);
        else
            ztrace_print_cont(coreID, "Updating next=%x for class=%d", new_next, size_class);
#endif
    }

    void trace_prefetch_next_end(bool success) {
#ifdef ZTRACE
        if (success)
            ztrace_print_finish(coreID, " succeeded.");
        else
            ztrace_print_finish(coreID, " failed.");
#endif
    }

    void trace_invalidate(size_t size_class) {
#ifdef ZTRACE
        ztrace_print(curr_uop, "e|SCC|Invalidating head & next for class=%d", size_class);
#endif
    }

};

#endif
