#include <cstdlib>
#include <cstring>

#include "misc.h"
#include "sim.h"
#include "stats.h"

#include "zesto-core.h"
#include "zesto-noc.h"

/* create a generic bus that can be used to connect one or more caches */
struct bus_t* bus_create(const char* const name,
                         const int width,
                         const tick_t* clock,
                         const int ratio) {
    struct bus_t* bus = (struct bus_t*)calloc(1, sizeof(*bus));
    if (!bus)
        fatal("failed to calloc bus %s", name);
    bus->name = strdup(name);
    bus->width = width;
    bus->clock = clock;
    bus->ratio = ratio;
    return bus;
}

void bus_reg_stats(xiosim::stats::StatsDatabase* sdb,
                   struct core_t* const core,
                   struct bus_t* const bus) {
    int core_id = xiosim::INVALID_CORE;
    if (core)
        core_id = core->id;
    if (!bus)
        return;

    auto sim_cycle_st = stat_find_stat<tick_t>(sdb, "sim_cycle");

    auto& accesses_st = stat_reg_comp_counter(sdb, true, core_id, bus->name, "accesses",
                                              "number of accesses to bus %s",
                                              &bus->stat.accesses, 0, true, NULL);

    auto& utilization_st = stat_reg_comp_double(sdb, true, core_id, bus->name, "utilization",
                                                "cumulative cycles of utilization of bus %s",
                                                &bus->stat.utilization, 0, true, NULL);
    stat_reg_comp_formula(sdb, true, core_id, bus->name, "avg_burst",
                          "avg cycles utilized per transfer of bus %s",
                          utilization_st / accesses_st, "%12.4f");

    stat_reg_comp_formula(sdb, true, core_id, bus->name, "duty_cycle",
                          "fraction of time bus %s was in use",
                          utilization_st / *sim_cycle_st, "%12.4f");

    auto& pf_utilization_st =
            stat_reg_comp_double(sdb, true, core_id, bus->name, "pf_utilization",
                                 "cumulative cycles of utilization of bus %s for prefetches",
                                 &bus->stat.prefetch_utilization, 0, true, NULL);
    stat_reg_comp_formula(sdb, true, core_id, bus->name, "pf_duty_cycle",
                          "fraction of time bus %s was in use for prefetches",
                          pf_utilization_st / *sim_cycle_st, "%12.4f");
}

/* Returns true is the bus is available */
int bus_free(const struct bus_t* const bus) {
    /* HACEDY HACKEDY HACK -- magic FSB */
    if (uncore_knobs.fsb_magic && strncmp(bus->name, "FSB", 3) == 0)
        return true;
    /* assume bus clock is locked to appropriate clock (synchronous): only
       allow operations when cycle MOD bus-multiplier is zero */
    if (*bus->clock % bus->ratio)
        return false;
    else
        return (bus->when_available <= *bus->clock);
}

/* Make use of the bus, thereby making it NOT free for some number of cycles hence */
void bus_use(struct bus_t* const bus, const int transfer_size, const int prefetch) {
    const double latency = ((transfer_size) / (double)bus->width) * bus->ratio;
    bus->when_available = (int)(*bus->clock + latency); /* round down*/
    bus->stat.accesses++;
    bus->stat.utilization += latency;
    if (prefetch)
        bus->stat.prefetch_utilization += latency;
}
