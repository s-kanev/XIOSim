#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(power_opt_string,"none"))
    return new core_power_NONE_t(core);
#else

class core_power_NONE_t : public core_power_t {

  public:
  core_power_NONE_t(struct core_t *_core) : core_power_t(core) { }

  virtual void translate_params(system_core *core_params, system_L2 *L2_params) { }
  virtual void translate_stats(struct stat_sdb_t* sdb, system_core* core_stats, system_L2 *L2_stats) { }
};

#endif
