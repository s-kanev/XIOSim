#ifndef ZESTO_POWER_INCLUDED
#define ZESTO_POWER_INCLUDE

void init_power(void);
void deinit_power(void);
void compute_power(struct stat_sdb_t* sdb, bool print_power);

#include "XML_Parse.h"

class core_power_t {

  public:
  core_power_t(struct core_t * _core);
  virtual ~core_power_t(void);

  double rt_power;

  virtual void translate_params(system_core *core_params, system_L2 *L2_params);
  virtual void translate_stats(struct stat_sdb_t* sdb, system_core *core_stats, system_L2 *L2_stats);

  protected:
  struct  core_t *core;
};

void power_reg_options(struct opt_odb_t *odb, struct core_knobs_t * knobs);
class core_power_t * power_create(const char * power_opt_string, struct core_t * core);

#endif /*ZESTO_POWER*/
