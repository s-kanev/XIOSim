/* none.cpp - No coherency */

#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(controller_opt_string,"none"))
    return new cache_controller_none_t(core, cache, controller_opt_string);
#else

class cache_controller_none_t : public cache_controller_t {
  public:
  cache_controller_none_t(struct core_t * const core, struct cache_t * const cache, const char * opt_string);


  virtual controller_array_response_t check_array(struct cache_line_t * line);
  virtual controller_response_t check_MSHR(struct cache_action_t * MSHR_item);

  virtual bool on_new_MSHR(int bank, int MSHR_index, struct cache_action_t * MSHR);
};

cache_controller_none_t::cache_controller_none_t(struct core_t * const core, struct cache_t * const cache, const char * opt_string) :
  cache_controller_t(core, cache)
{
  (void) opt_string;
}

controller_array_response_t cache_controller_none_t::check_array(struct cache_line_t * line)
{
  if (line == NULL)
    return ARRAY_MISS;

  return ARRAY_HIT;
}

controller_response_t cache_controller_none_t::check_MSHR(struct cache_action_t * MSHR_item)
{
  (void) MSHR_item;

  return MSHR_CHECK_ARRAY;
}

bool cache_controller_none_t::on_new_MSHR(int bank, int MSHR_index, struct cache_action_t * MSHR)
{
  if(cache->next_level) /* enqueue the request to the next-level cache */
  {
    if(!cache_enqueuable(cache->next_level, DO_NOT_TRANSLATE, MSHR->paddr))
      return false;

    cache_enqueue(MSHR->core, cache->next_level, cache,MSHR->cmd, DO_NOT_TRANSLATE, MSHR->PC, MSHR->paddr, MSHR->action_id, bank, MSHR_index, MSHR->op, MSHR->cb, MSHR->miss_cb, NULL, MSHR->get_action_id);

    bus_use(cache->next_bus, (MSHR->cmd == CACHE_WRITE || MSHR->cmd == CACHE_WRITEBACK) ? cache->linesize : 1, MSHR->cmd == CACHE_PREFETCH);
  }
  else /* or if there is no next level, enqueue to the memory controller */
  {
    if(!uncore->MC->enqueuable())
      return false;

    uncore->MC->enqueue(cache, MSHR->cmd, MSHR->paddr, cache->linesize, MSHR->action_id, bank, MSHR_index, MSHR->op, MSHR->cb, MSHR->get_action_id);

    bus_use(uncore->fsb, (MSHR->cmd == CACHE_WRITE || MSHR->cmd == CACHE_WRITEBACK) ? (cache->linesize>>uncore->fsb_DDR) : 1, MSHR->cmd == CACHE_PREFETCH);
  }

  MSHR->when_started = sim_cycle;
  return true;
}

#endif /* ZESTO_PARSE_ARGS */
