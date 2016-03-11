#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(exec_opt_string,"none"))
    return std::make_unique<class core_exec_NONE_t>(core);
#else

class core_exec_NONE_t:public core_exec_t
{
  public:

  core_exec_NONE_t(struct core_t * const core);
  virtual void reg_stats(xiosim::stats::StatsDatabase* sdb);
  virtual void LDST_exec(void);
  virtual void ALU_exec(void) {};

  /* All stubs */
  virtual void freeze_stats(void) { }
  virtual void update_occupancy(void) { }
  virtual void reset_execution(void) { }

  virtual void RS_schedule(void) { }
  virtual void LDQ_schedule(void) { }

  virtual void recover(const struct Mop_t * const Mop) { }
  virtual void recover(void) { }

  virtual bool LDQ_available(void) { return false; }
  virtual void LDQ_insert(struct uop_t * const uop) { }
  virtual void LDQ_deallocate(struct uop_t * const uop) { }
  virtual void LDQ_squash(struct uop_t * const dead_uop) { }

  virtual bool STQ_available(void) { return false; }
  virtual void STQ_insert_sta(struct uop_t * const uop) { }
  virtual void STQ_insert_std(struct uop_t * const uop) { }
  virtual void STQ_deallocate_sta(void) { }
  virtual bool STQ_deallocate_std(struct uop_t * const uop) { return false; }
  virtual void STQ_deallocate_senior(void) { }
  virtual void STQ_squash_sta(struct uop_t * const dead_uop) { }
  virtual void STQ_squash_std(struct uop_t * const dead_uop) { }
  virtual void STQ_squash_senior(void) { }
  virtual bool STQ_empty(void) { return true; }
  virtual void STQ_set_addr(struct uop_t * const uop) {}
  virtual void STQ_set_data(struct uop_t * const uop) {}

  virtual void recover_check_assertions(void) { }

  virtual void step(void) { }
  virtual void exec_fuse_insert(struct uop_t * const uop) { }
  virtual bool exec_empty(void) { return true; }
  virtual void exec_insert(struct uop_t * const uop) { }
  virtual bool port_available(int port_ind) { return false; }
  virtual bool exec_fused_ST(struct uop_t * const uop) { return false; }

  virtual void insert_ready_uop(struct uop_t * const uop) { }
  virtual bool RS_available(void) { return false; }
  virtual void RS_insert(struct uop_t * const uop) { }
  virtual void RS_fuse_insert(struct uop_t * const uop) { }
  virtual void RS_deallocate(struct uop_t * const uop) { }

  static void DL1_load_callback(void * const op);
  static void DL1_store_callback(void * const op);
  static bool translated_callback(void * const op, const seq_t action_id);
  static seq_t get_uop_action_id(void * const op);
};

core_exec_NONE_t::core_exec_NONE_t(class core_t* const arg_core)
    : core_exec_t(arg_core) {
    create_caches(false);
}

void core_exec_NONE_t::reg_stats(xiosim::stats::StatsDatabase* sdb) { reg_dcache_stats(sdb); }

void core_exec_NONE_t::DL1_load_callback(void * const op)
{
  struct uop_t * uop = (struct uop_t*) op;
  class core_t * core = uop->core;
  // Mark load as done with caches.
  uop->timing.when_completed = core->sim_cycle;
}

void core_exec_NONE_t::DL1_store_callback(void * const op) {}
bool core_exec_NONE_t::translated_callback(void * const op, const seq_t action_id) { return true; }
seq_t core_exec_NONE_t::get_uop_action_id(void * const op) { return 1; }

void core_exec_NONE_t::LDST_exec(void)
{
  // Step caches.
  step_dcaches();

  // We only process the oldest Mop in program order (max IPC = 1).
  struct Mop_t* Mop = core->oracle->get_oldest_Mop();
  if (!Mop)
    return;

  // Mops that don't touch memory get to commit straight away.
  if (!Mop->decode.opflags.MEM) {
    Mop->timing.when_commit_finished = core->sim_cycle;
    return;
  }

  for (size_t i = 0; i < Mop->decode.flow_length; i++) {
    uop_t* uop = &Mop->uop[i];

    // Skip immediates.
    if (uop->decode.is_imm)
      continue;

    // Anything that's not a load or sta gets to complete.
    if (!uop->decode.is_load && !uop->decode.is_sta) {
      uop->timing.when_completed = core->sim_cycle;
      continue;
    }

    // Don't schedule the same load or store twice.
    if (uop->timing.when_exec != TICK_T_MAX || uop->timing.when_completed != TICK_T_MAX)
      continue;

    // Finally schedule request to the cache
    cache_command cache_op = uop->decode.is_load ? CACHE_READ : CACHE_WRITE;
    md_addr_t addr = uop->oracle.virt_addr;
    md_addr_t pc = Mop->fetch.PC;
    int asid = core->asid;

    if (cache_enqueuable(core->memory.DL1.get(), asid, addr)) {
      // Mark uop as sent to caches
      uop->timing.when_exec = core->sim_cycle;

      if (cache_op == CACHE_READ) {
        cache_enqueue(core, core->memory.DL1.get(), NULL, CACHE_READ, asid, pc, addr, 1, 0, NO_MSHR, uop, DL1_load_callback, NULL, translated_callback, get_uop_action_id);
      } else {
        // For writeback caches, we mark them as completed right away,
        // without waiting for them to go through the cache hierachy.
        cache_enqueue(core, core->memory.DL1.get(), NULL, CACHE_WRITE, asid, pc, addr, 1, 0, NO_MSHR, NULL, DL1_store_callback, NULL, translated_callback, get_uop_action_id);
        uop->timing.when_completed = core->sim_cycle;
      }
    }
  }
}

#endif
