#ifndef ZESTO_POWER_INCLUDED
#define ZESTO_POWER_INCLUDED

#include <memory>

/* McPAT interface fwd */
struct system_core;
struct system_L2;

void init_power(void);
void deinit_power(void);
void compute_power(xiosim::stats::StatsDatabase* sdb, bool print_power);

class core_power_t {

  public:
  core_power_t(struct core_t * _core);
  virtual ~core_power_t(void) {}

  double rt_power;
  double leakage_power;
  static double default_vdd;

  virtual void translate_params(system_core *core_params, system_L2 *L2_params);
  virtual void translate_stats(xiosim::stats::StatsDatabase* sdb,
                               system_core* core_stats,
                               system_L2* L2_stats);

  protected:
  struct  core_t *core;
};

std::unique_ptr<class core_power_t> power_create(const char * power_opt_string, struct core_t * core);

#endif /*ZESTO_POWER*/
