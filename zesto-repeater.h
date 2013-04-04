/* zesto-repeater.h - Ring cache code base classes */
/*
 * Svilen Kanev, 2013
 */

#ifndef __ZESTO_REPEATER_H
#define __ZESTO_REPEATER_H

class repeater_t {
  public:
    struct core_t * core;
    const char * const name;
    struct cache_t * nextLevel;

    repeater_t(struct core_t * const _core, const char * const _name, struct cache_t * const _next_level) :
        core(_core), name(_name), nextLevel(_next_level) { }

    /* Called every cycle. Repeater processing happens here. */
    virtual void step() = 0;

    /* Checks if we can insert request in repeater. Returns 1 if possible */
    virtual int enqueuable(const enum cache_command cmd, const int thread_id, const md_addr_t addr) = 0;

    /* Send a request to the repeater.
       Assumes repeater_enqueuable() has been called in the same cycle.
       cb is called with op once request is not blocking any more. */
    virtual void enqueue(const enum cache_command cmd,
                const int thread_id,
                const md_addr_t addr,
                void * const op,    /* To be passed to callback for identification */
                void (*const cb)(void *, bool is_hit), /* Callback once request is finished */
                seq_t (*const get_action_id)(void* const)) = 0;

    /* Flushes all of repeater contents.
       Some addresses are propagated to next level cache.
       cb is called once flush is finished. */
    virtual void flush(void (*const cb)()) = 0;
};

/* Initialize the repeater.  */
class repeater_t * repeater_create(
    const char * const opt_string,
    struct core_t * const core,
    const char * const name,
    struct cache_t * const next_level);

void repeater_init(const char * const opt_string);
void repeater_shutdown(const char * const opt_string);
void repeater_reg_options(struct opt_odb_t * const odb);

#endif /*__ZESTO_REPEATER_H */
