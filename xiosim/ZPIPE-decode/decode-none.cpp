#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(decode_opt_string,"none"))
    return std::make_unique<class core_decode_NONE_t>(core);
#else

class core_decode_NONE_t:public core_decode_t
{
  public:

  /* constructor, stats registration */
  core_decode_NONE_t(struct core_t * const core) { }
  virtual void reg_stats(xiosim::stats::StatsDatabase* sdb) { }
  virtual void update_occupancy(void) { }

  virtual void step(void) { }
  virtual void recover(void) { }
  virtual void recover(struct Mop_t * const Mop) { }

  /* interface functions for alloc stage */
  virtual bool uop_available(void) { return false; }
  virtual struct uop_t * uop_peek(void) { return NULL; }
  virtual void uop_consume(void) { }
};

#endif
