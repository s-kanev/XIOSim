#ifndef ZESTO_COHERENCE_INCLUDED
#define ZESTO_COHERENCE_INCLUDED

class cache_controller_t {
  public:
  cache_controller_t (
    struct core_t * const core,
    struct cache_t * const cache);

  virtual bool is_hit (
    struct cache_action_t * mshr_item,
    const enum cache_command cmd,
    const md_paddr_t addr) = 0;

  bool use_victim_cache;

  protected:
  struct cache_t * const cache;
  struct core_t * const core;

};

class LLC_controller_t {

};

class cache_controller_t * controller_create(const char * controller_opt_string, struct core_t * core, struct cache_t * cache);

#endif /*ZESTO_COHERENCE*/
