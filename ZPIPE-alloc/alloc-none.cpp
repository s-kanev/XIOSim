#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(alloc_opt_string,"none"))
    return new core_alloc_NONE_t(core);
#else

class core_alloc_NONE_t:public core_alloc_t
{

  public:

  core_alloc_NONE_t(struct core_t * const core) { }
  virtual void reg_stats(xiosim::stats::StatsDatabase* sdb) { }

  virtual void step(void) { }
  virtual void recover(void) { }
  virtual void recover(const struct Mop_t * const Mop) { }

  virtual void RS_deallocate(const struct uop_t * const uop) { }
  virtual void start_drain(void) { } /* prevent allocation from proceeding to exec */
};

#endif
