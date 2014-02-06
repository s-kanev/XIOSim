/* repeater-none.cpp - Stub for non-existent ring cache */
/*
 * Svilen Kanev, 2013
 */
#define COMPONENT_NAME "none"

#ifdef REPEATER_PARSE_ARGS
if(!strcasecmp(COMPONENT_NAME,type))
{
  return new repeater_none_t(core, name, next_level);
}

#elif defined(REPEATER_INIT)
if(!strcasecmp(COMPONENT_NAME,type))
{
  return;
}

#elif defined(REPEATER_SHUTDOWN)
if(!strcasecmp(COMPONENT_NAME,type))
{
  return;
}

#else
class repeater_none_t: public repeater_t {
  public:
    repeater_none_t(struct core_t * const _core, const char * const _name, struct cache_t * const _next_level) : repeater_t (_core, _name, _next_level) { speed = 0.0; };
    virtual void step() { };
    virtual int enqueuable(const enum cache_command cmd, const int asid, const md_addr_t addr) { return false; }
    virtual void enqueue(const enum cache_command cmd,
                const int asid,
                const md_addr_t addr,
                void * const op,    /* To be passed to callback for identification */
                void (*const cb)(void *, bool is_hit), /* Callback once request is finished */
                seq_t (*const get_action_id)(void* const)) { if (cb) cb(op, true); };

    virtual void flush(void (*const cb)()) { if (cb) cb(); };
};

#endif

#undef COMPONENT_NAME
