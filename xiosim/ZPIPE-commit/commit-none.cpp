#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(commit_opt_string,"none"))
    return std::make_unique<class core_commit_NONE_t>(core);
#else

class core_commit_NONE_t:public core_commit_t
{
  public:

  core_commit_NONE_t(struct core_t * const core) { this->core = core; }
  virtual void reg_stats(xiosim::stats::StatsDatabase* sdb);
  virtual void update_occupancy(void) { }

  virtual void step(void) {}
  virtual void IO_step(void);
  virtual void recover(const struct Mop_t * const Mop) { }
  virtual void recover(void) { }

  virtual bool ROB_available(void) { return false; }
  virtual bool ROB_empty(void) { return true; }
  virtual bool pipe_empty(void) { return true; }
  virtual void ROB_insert(struct uop_t * const uop) { }
  virtual void ROB_fuse_insert(struct uop_t * const uop) { }

  virtual void pre_commit_insert(struct uop_t * const uop) { }
  virtual void pre_commit_fused_insert(struct uop_t * const uop) { }
  virtual bool pre_commit_available() { return false; }
  virtual void pre_commit_step() { }
  virtual void pre_commit_recover(struct Mop_t * const Mop) { }
  virtual int squash_uop(struct uop_t * const uop) { return 0; }
};

void core_commit_NONE_t::reg_stats(xiosim::stats::StatsDatabase* sdb) {
    int coreID = core->id;
    stat_reg_note(sdb, "\n#### COMMIT STATS ####");

    auto& sim_cycle_st = *stat_find_core_stat<tick_t>(sdb, coreID, "sim_cycle");
    auto& commit_insn_st = *stat_find_core_stat<counter_t>(sdb, coreID, "commit_insn");
    stat_reg_core_formula(sdb, true, coreID, "commit_IPC", "IPC at commit",
                          commit_insn_st / sim_cycle_st, NULL);
}

void core_commit_NONE_t::IO_step()
{
  // We only process the oldest Mop in program order (max IPC = 1).
  Mop_t* Mop = core->oracle->get_oldest_Mop();
  if (!Mop)
    return;

  // Not complete, or partially complete.
  if (Mop->timing.when_commit_finished > core->sim_cycle) {
    // Partially complete, have to check all uops.
    bool all_completed = true;
    for (size_t i = 0; i < Mop->decode.flow_length; i++) {
      uop_t * uop = &Mop->uop[i];
      if (uop->decode.is_imm)
        continue;
      if (uop->timing.when_completed > core->sim_cycle)
        all_completed = false;
    }
    if (!all_completed)
      return;
  }

  // Mop is complete, commit all uops.
  for (size_t i = 0; i < Mop->decode.flow_length; i++) {
    struct uop_t* uop = &Mop->uop[i];
    if (!uop->decode.is_imm) {

#ifdef ZTRACE
      ztrace_print(uop,"c|commit|uop committed");
#endif
      core->oracle->commit_uop(&Mop->uop[i]);
    }
  }
#ifdef ZTRACE
  ztrace_print(Mop,"c|commit|all uops in Mop committed; Mop retired");
#endif

  // ... and the Mop itself.
  core->oracle->commit(Mop);
  core->stat.commit_insn++;
}

#endif
