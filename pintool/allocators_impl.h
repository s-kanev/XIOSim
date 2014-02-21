#ifndef __ALLOCATORS_IMPL_H__
#define __ALLOCATORS_IMPL_H__

#include <string>
#include <vector>
#include <assert.h>

#include "base_allocator.h"

namespace xiosim {

/* Trivial allocator that always allocates X cores. */
class GangAllocator : public BaseAllocator {
  public:
    /* Gets a singleton allocator for a system with @ncores cores. */
    static GangAllocator& Get(int ncores) {
        static GangAllocator instance(ncores);
        return instance;
    }
    ~GangAllocator();
    int AllocateCoresForProcess(int asid, std::vector<double> scaling);

  private:
    GangAllocator(int num_cores);
};

/* A credit-based core allocator. */
class PenaltyAllocator : public BaseAllocator {
  public:
    /* Gets a singleton allocator for a system with @ncores cores. */
    static PenaltyAllocator& Get(int ncores) {
        static PenaltyAllocator instance(ncores);
        return instance;
    }
    int AllocateCoresForProcess(int asid, std::vector<double> scaling);
    // Returns the current penalty on process asid, or -1 if the process does
    // not exist in the allocator's knowledge.
    double get_penalty_for_asid(int asid);

  private:
    PenaltyAllocator(int num_cores);
    std::map<int, double> *process_penalties;
    // Double precision floating point comparison accuracy.
    double SPEEDUP_EPSILON = 0.000001;
};

/* Locally optimal allocator that waits for all loops to align before making the
 * next allocation decision.
 */
class LocallyOptimalAllocator : public BaseAllocator {
  public:
    /* Gets a singleton allocator for a system with @ncores cores. */
    static LocallyOptimalAllocator& Get(int ncores) {
        static LocallyOptimalAllocator instance(ncores);
        return instance;
    }
    ~LocallyOptimalAllocator();
    int AllocateCoresForProcess(int asid, std::vector<double> scaling);
  private:
    LocallyOptimalAllocator(int num_cores);
    struct process_sync_t {
      int num_checked_in;
      int num_checked_out;
      bool allocation_complete;
    } process_sync;
    std::vector<std::vector<double>*> process_scaling;

    // Get/reset all class global variables for sharing among threads.
    void ResetState();
};

class AllocatorParser {
  public:
    static BaseAllocator& Get(std::string opts, int num_cores) {
      if (opts.find("gang") != opts.npos) {
        char name[16];
        int max_cores;
        if (sscanf(opts.c_str(), "%[^:]:%d", name, &max_cores) != 2)
            assert(false);
        assert(max_cores > 0 && max_cores <= num_cores);
        return GangAllocator::Get(max_cores);
      }
      if (opts == "local")
        return LocallyOptimalAllocator::Get(num_cores);
      if (opts == "penalty")
        return PenaltyAllocator::Get(num_cores);
      assert(false);
    }
};

}  // namespace xiosim

#endif
