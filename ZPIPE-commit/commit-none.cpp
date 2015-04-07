#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(commit_opt_string,"none"))
    return new core_commit_NONE_t(core);
#else

class core_commit_NONE_t:public core_commit_t
{
  public:

  core_commit_NONE_t(struct core_t * const core) { this->core = core; }
  virtual void reg_stats(struct stat_sdb_t * const sdb);
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

void core_commit_NONE_t::reg_stats(struct stat_sdb_t * const sdb)
{
  char buf[1024];
  char buf2[1024];
  struct thread_t * arch = core->current_thread;

  stat_reg_note(sdb,"\n#### COMMIT STATS ####");

  sprintf(buf,"c%d.sim_cycle",arch->id);
  stat_reg_qword(sdb, true, buf, "total number of cycles when last instruction (or uop) committed", (qword_t*) &core->sim_cycle, 0, TRUE, NULL);
  sprintf(buf,"c%d.commit_insn",arch->id);
  stat_reg_counter(sdb, true, buf, "total number of instructions committed", &core->stat.commit_insn, 0, TRUE, NULL);
  sprintf(buf,"c%d.commit_IPC",arch->id);
  sprintf(buf2,"c%d.commit_insn/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "IPC at commit", buf2, NULL);
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
    for (int i = 0; i < Mop->decode.flow_length; i++) {
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
  lk_lock(&memory_lock, 1);
  for (int i = 0; i < Mop->decode.flow_length; i++) {
    if (!Mop->uop[i].decode.is_imm) {
      core->oracle->commit_uop(&Mop->uop[i]);
    }
  }
  lk_unlock(&memory_lock);
  // ... and the Mop itself.
  core->oracle->commit(Mop);
  core->stat.commit_insn++;
}

#endif
