#ifndef ZESTO_COHERENCE_INCLUDED
#define ZESTO_COHERENCE_INCLUDED

enum controller_response_t { MSHR_CHECK_ARRAY, MSHR_STALL };
enum controller_array_response_t { ARRAY_HIT, ARRAY_MISS };

class cache_controller_t {
  public:
  cache_controller_t (
    struct core_t * const core,
    struct cache_t * const cache);

  virtual controller_array_response_t check_array(struct cache_line_t * line) = 0;
  virtual controller_response_t check_MSHR(struct cache_action_t * MSHR_item) = 0;

  virtual bool on_new_MSHR(int bank, int MSHR_index, struct cache_action_t * MSHR) = 0;

  protected:
  struct cache_t * const cache;
  struct core_t * const core;

};

class LLC_controller_t {

};

class cache_controller_t * controller_create(const char * controller_opt_string, struct core_t * core, struct cache_t * cache);

#endif /*ZESTO_COHERENCE*/
