#ifndef ZESTO_COHERENCE_INCLUDED
#define ZESTO_COHERENCE_INCLUDED

class cache_controller_t {
  public:
  cache_controller_t (
    struct core_t * const core,
    struct cache_t * const cache);

  virtual bool is_hit (
    struct cache_line_t * array_line,
    const enum cache_command cmd,
    const md_paddr_t addr) = 0;

  protected:
  struct cache_t * const cache;
  struct core_t * const core;

};

class LLC_controller_t {

};

#endif /*ZESTO_COHERENCE*/
