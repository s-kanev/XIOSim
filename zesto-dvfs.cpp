#include "zesto-structs.h"
#include "zesto-core.h"
#include "zesto-power.h"
#include "ztrace.h"
#include "zesto-dvfs.h"

using namespace std;

/* load in all definitions */
#include "ZCOMPS-dvfs.list"

class vf_controller_t * vf_controller_create(const char * opt_string, struct core_t * core)
{
#define ZESTO_PARSE_ARGS
#include "ZCOMPS-dvfs.list"
#undef ZESTO_PARSE_ARGS

  fatal("unknown dvfs controller type (%s)", opt_string);
}

double vf_controller_t::get_average_vdd()
{
  pair<tick_t, double> curr, next;
  double sum;

  if (voltages.empty()) {
    last_power_computation = core->sim_cycle;
    return vdd;
  }

  if (last_power_computation == core->sim_cycle) {
    return vdd;
  }

  curr = voltages.front();
  sum = (curr.first - last_power_computation) * curr.second;

  while(voltages.size() > 1) {
    voltages.pop();
    next = voltages.front();

    sum += (next.first - curr.first) * curr.second;
    curr = next;
  }

  curr = voltages.front();
  sum += (core->sim_cycle - curr.first) * curr.second;
  sum /= (core->sim_cycle - last_power_computation);
  last_power_computation = core->sim_cycle;
  return sum;
}

void vf_controller_t::change_vf()
{
  voltages.push(pair<tick_t, double>(core->sim_cycle, vdd));
}
