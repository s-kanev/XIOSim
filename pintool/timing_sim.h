#ifndef __TIMING_SIM__
#define __TIMING_SIM__

/* ========================================================================== */
class sim_thread_state_t {
public:
    sim_thread_state_t() {

        is_running = true;
        // Will get cleared on first simulated instruction
        sim_stopped = true;

        lk_init(&lock);
    }

    XIOSIM_LOCK lock;
    // XXX: SHARED -- lock protects those
    // Signal to the simulator thread to die
    BOOL is_running;
    // Set by simulator thread once it dies
    BOOL sim_stopped;
    // XXX: END SHARED
};
sim_thread_state_t* get_sim_tls(THREADID tid);

#endif /* __TIMING_SIM__ */
