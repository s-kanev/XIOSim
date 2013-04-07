#include "thread.h"

#include "zesto-opts.h"
#include "zesto-core.h"
#include "zesto-noc.h"

extern bool fsb_magic;

/* create a generic bus that can be used to connect one or more caches */
struct bus_t * bus_create(
    const char * const name,
    const int width,
    const tick_t * clock,
    const int ratio)
{
  struct bus_t * bus = (struct bus_t*) calloc(1,sizeof(*bus));
  if(!bus)
    fatal("failed to calloc bus %s",name);
  bus->name = strdup(name);
  bus->width = width;
  bus->clock = clock;
  bus->ratio = ratio;
  return bus;
}

void bus_reg_stats(
    struct stat_sdb_t * const sdb,
    struct core_t * const core,
    struct bus_t * const bus)
{
  char core_str[256];
  if(core)
    sprintf(core_str,"c%d.",core->current_thread->id);
  else
    core_str[0] = '\0'; /* empty string */

  char buf[256];
  char buf2[256];
  char buf3[256];

  sprintf(buf,"%s%s.accesses",core_str,bus->name);
  sprintf(buf2,"number of accesses to bus %s",bus->name);
  stat_reg_counter(sdb, true, buf, buf2, &bus->stat.accesses, 0, TRUE, NULL);
  sprintf(buf,"%s%s.utilization",core_str,bus->name);
  sprintf(buf2,"cumulative cycles of utilization of bus %s",bus->name);
  stat_reg_counter(sdb, true, buf, buf2, &bus->stat.utilization, 0, TRUE, NULL);
  sprintf(buf,"%s%s.avg_burst",core_str,bus->name);
  sprintf(buf2,"avg cycles utilized per transfer of bus %s",bus->name);
  sprintf(buf3,"%s%s.utilization/%s%s.accesses",core_str,bus->name,core_str,bus->name);
  stat_reg_formula(sdb, true, buf, buf2, buf3, "%12.4f");
  sprintf(buf,"%s%s.duty_cycle",core_str,bus->name);
  sprintf(buf2,"fraction of time bus %s was in use",bus->name);
  sprintf(buf3,"%s%s.utilization/sim_cycle",core_str,bus->name);
  stat_reg_formula(sdb, true, buf, buf2, buf3, "%12.4f");
  sprintf(buf,"%s%s.pf_utilization",core_str,bus->name);
  sprintf(buf2,"cumulative cycles of utilization of bus %s for prefetches",bus->name);
  stat_reg_counter(sdb, true, buf, buf2, &bus->stat.prefetch_utilization, 0, TRUE, NULL);
  sprintf(buf,"%s%s.pf_duty_cycle",core_str,bus->name);
  sprintf(buf2,"fraction of time bus %s was in use for prefetches",bus->name);
  sprintf(buf3,"%s%s.pf_utilization/sim_cycle",core_str,bus->name);
  stat_reg_formula(sdb, true, buf, buf2, buf3, "%12.4f");
}

/* Returns true is the bus is available */
int bus_free(const struct bus_t * const bus)
{
  /* HACEDY HACKEDY HACK -- magic FSB */
  if(fsb_magic && !strcmp(bus->name, "FSB"))
    return true;
  /* assume bus clock is locked to cpu clock (synchronous): only
     allow operations when cycle MOD bus-multiplier is zero */
  if(*bus->clock % bus->ratio)
    return false;
  else
    return (bus->when_available <= *bus->clock);
}

/* Make use of the bus, thereby making it NOT free for some number of cycles hence */
void bus_use(
    struct bus_t * const bus,
    const int transfer_size,
    const int prefetch)
{
  const int latency = (((transfer_size-1) / bus->width)+1) * bus->ratio; /* round up */
  bus->when_available = *bus->clock + latency;
  bus->stat.accesses++;
  bus->stat.utilization += latency;
  if(prefetch)
    bus->stat.prefetch_utilization += latency;
}
