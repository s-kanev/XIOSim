/* power-DPM.cpp - Power proxy for OoO core */


#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(power_opt_string,"DPM"))
    return new core_power_DPM_t(core);
#else

class core_power_DPM_t : public core_power_t {

  public:
  core_power_DPM_t(struct core_t *_core);

  void translate_params(system_core *core_params);
  void translate_stats(system_core* core_stats);

  protected:
  struct core_t * core;
};

core_power_DPM_t::core_power_DPM_t(struct core_t *_core):
  core(_core)
{
}

void core_power_DPM_t::translate_params(system_core *core_params)
{
}

void core_power_DPM_t::translate_stats(system_core *core_stats)
{
}
#endif
