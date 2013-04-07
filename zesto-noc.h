#ifndef ZESTO_NOC_INCLUDED
#define ZESTO_NOC_INCLUDED

struct bus_t {
  char * name;
  int width; /* in bytes tranferrable per cycle */
  const tick_t * clock; /* The sim_cycle used to drive this bus */
  int ratio; /* number of ^ clock cycles per bus cycle */
  tick_t when_available;

  struct {
    counter_t accesses;
    counter_t utilization; /* cumulative cycles in use */
    counter_t prefetch_utilization; /* cumulative prefetch cycles in use */
  } stat;
};

struct bus_t * bus_create(
    const char * const name,
    const int width,
    const tick_t * clock,
    const int ratio);

void bus_reg_stats(
    struct stat_sdb_t * const sdb,
    struct core_t * const core,
    struct bus_t * const bus);

int bus_free(const struct bus_t * const bus);

void bus_use(
    struct bus_t * const bus,
    const int transfer_size,
    const int prefetch);

#endif /* ZESTO_NOC */
