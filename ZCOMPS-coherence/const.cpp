/* const.cpp - Const-latency coherency */

#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(controller_opt_string,"const"))
    return new cache_controller_const_t(core, cache, controller_opt_string);
#else

class cache_controller_const_t : public cache_controller_t {
  public:
  cache_controller_const_t(struct core_t * const core, struct cache_t * const cache, const char * opt_string);


  virtual controller_array_response_t check_array(struct cache_line_t * line);
  virtual controller_response_t check_MSHR(struct cache_action_t * MSHR_item);

  virtual bool can_schedule_upstream();
  virtual bool can_schedule_downstream(struct cache_t * const prev_cache);
  virtual bool on_new_MSHR(int bank, int MSHR_index, struct cache_action_t * MSHR);
};

struct const_coherence_data : public line_coherence_data_t {
  bool is_used() { return v > 0; }
};

cache_controller_const_t::cache_controller_const_t(struct core_t * const core, struct cache_t * const cache, const char * opt_string) :
  cache_controller_t(core, cache)
{
  (void) opt_string;
}

controller_array_response_t cache_controller_const_t::check_array(struct cache_line_t * line)
{
  if (line == NULL)
    return ARRAY_MISS;

  const_coherence_data * d = static_cast<const_coherence_data *>(&line->coh);
  return d->is_used() ? ARRAY_HIT : ARRAY_MISS;
}

controller_response_t cache_controller_const_t::check_MSHR(struct cache_action_t * MSHR_item)
{
  (void) MSHR_item;

  return MSHR_CHECK_ARRAY;
}

bool cache_controller_const_t::can_schedule_upstream()
{
  if (cache->next_level)
    return (!cache->next_bus || bus_free(cache->next_bus));
  return bus_free(uncore->fsb);
}

bool cache_controller_const_t::can_schedule_downstream(struct cache_t * const prev_cache)
{
  return (!prev_cache || bus_free(prev_cache->next_bus));
}

bool cache_controller_const_t::on_new_MSHR(int bank, int MSHR_index, struct cache_action_t * MSHR)
{
  return true;
}

#endif /* ZESTO_PARSE_ARGS */
