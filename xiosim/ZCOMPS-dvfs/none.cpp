/* none.cpp - No DVFS, just set frequency to default on create */

#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(opt_string, "none"))
    return std::make_unique<vf_controller_none_t>(core);
#else

class vf_controller_none_t : public vf_controller_t {
  public:
    vf_controller_none_t(struct core_t * const _core);

    virtual void change_vf();
};

vf_controller_none_t::vf_controller_none_t(struct core_t * const _core) : vf_controller_t(_core)
{
  core->cpu_speed = core->knobs->default_cpu_speed;
}

void vf_controller_none_t::change_vf()
{
  double old_vdd = vdd;
  
  //XXX: your fancy DVFS algorithm comes here

  // handle bookkeeping
  if (vdd != old_vdd)
    vf_controller_t::change_vf();
}

#endif
