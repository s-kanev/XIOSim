#ifndef __ZESTO_DVFS__
#define __ZESTO_DVFS__

#include <memory>
#include <queue>

class vf_controller_t {
  public:
    vf_controller_t (struct core_t * const _core) : next_invocation(0), core(_core) { }
    virtual ~vf_controller_t() { }

    virtual void change_vf();
    virtual double get_average_vdd();

    tick_t next_invocation;

    double vdd;

  protected:
    struct core_t * const core;
    tick_t last_power_computation;
    std::queue<std::pair<tick_t, double> > voltages;
};

std::unique_ptr<vf_controller_t> vf_controller_create(const char * opt_string, struct core_t * core);

#endif /* __ZESTO_DVFS__ */
