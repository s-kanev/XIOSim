#ifndef __ZESTO_DVFS__
#define __ZESTO_DVFS__

class vf_controller_t {
  public:
    vf_controller_t (struct core_t * const _core) : next_invocation(0), core(_core) { }

    virtual void change_vf() = 0;

    tick_t next_invocation;

  protected:
    struct core_t * const core;
};

class vf_controller_t * vf_controller_create(const char * opt_string, struct core_t * core);

#endif /* __ZESTO_DVFS__ */
