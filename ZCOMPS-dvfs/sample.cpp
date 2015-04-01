/* sample.cpp - Simple IPC-based frequency controller */

#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(opt_string, "sample"))
    return new vf_controller_sample_t(core);
#else

/* This is a simple policy to get the flavor of the DVFS API.
 * Not representative of anything realistic.
 * Calculate IPC on the fly. If below 0.6, go to half the default frequency.
 */
class vf_controller_sample_t : public vf_controller_t {
  public:
    vf_controller_sample_t(struct core_t * const _core);

    virtual void change_vf();

  protected:
    tick_t last_cycle;
    counter_t last_commit_insn;
    float min_freq;
    float max_freq;
};

vf_controller_sample_t::vf_controller_sample_t(struct core_t * const _core) : vf_controller_t(_core)
{
    last_cycle = 0;
    last_commit_insn = 0;
    max_freq = core->knobs->default_cpu_speed;
    min_freq = 0.5 * max_freq;

    core->cpu_speed = core->knobs->default_cpu_speed;
}

void vf_controller_sample_t::change_vf()
{
    counter_t delta_insn = core->stat.commit_insn - last_commit_insn;
    tick_t delta_cycles = core->sim_cycle - last_cycle;
    float curr_ipc = delta_insn / (float) delta_cycles;
    if (curr_ipc < 0.6 )
        core->cpu_speed = min_freq;
    else
        core->cpu_speed = max_freq;

    ZTRACE_PRINT(core->coreID, "cycles: %lld instrs: %lld IPC: %.3f cpu_speed: %.1f\n", delta_cycles, delta_insn, curr_ipc, core->cpu_speed);

    last_cycle = core->sim_cycle;
    last_commit_insn = core->stat.commit_insn;

    /* If you change voltages, update the base controller. */
    /*
    if (vdd != previous_vdd)
        vf_controller_t::change_vf();
    */
}

#endif
