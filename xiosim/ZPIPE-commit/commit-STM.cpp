/* commit-STM.cpp - Simple(r) Timing Model */
/*
 * __COPYRIGHT__ GT
 */

#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(commit_opt_string,"STM"))
    return std::make_unique<class core_commit_STM_t>(core);
#else

class core_commit_STM_t:public core_commit_t
{
  enum commit_stall_t {CSTALL_NONE,      /* no stall */
                       CSTALL_NOT_READY, /* oldest inst not done (no uops finished) */
                       CSTALL_PARTIAL,   /* oldest inst not done (but some uops finished) */
                       CSTALL_EMPTY,     /* ROB is empty, nothing to commit */
                       CSTALL_num
                     };

  public:

  core_commit_STM_t(struct core_t * const core);
  ~core_commit_STM_t();
  virtual void reg_stats(xiosim::stats::StatsDatabase* sdb);
  virtual void update_occupancy(void);

  virtual void step(void);
  virtual void IO_step(void);
  virtual void recover(const struct Mop_t * const Mop);
  virtual void recover(void);

  virtual bool ROB_available(void);
  virtual bool ROB_empty(void);
  virtual bool pipe_empty(void);
  virtual void ROB_insert(struct uop_t * const uop);
  virtual void ROB_fuse_insert(struct uop_t * const uop);

  virtual void pre_commit_insert(struct uop_t * const uop);
  virtual void pre_commit_fused_insert(struct uop_t * const uop);
  virtual bool pre_commit_available();
  virtual void pre_commit_step();
  virtual void pre_commit_recover(struct Mop_t * const Mop);
  virtual void squash_uop(struct uop_t * const uop);

  protected:

  struct uop_t ** ROB;
  int ROB_head;
  int ROB_tail;
  int ROB_num;

  static const char *commit_stall_str[CSTALL_num];

  /* additional temps to track timing of REP insts */
  tick_t when_rep_fetch_started;
  tick_t when_rep_fetched;
  tick_t when_rep_decode_started;
  tick_t when_rep_commit_started;
};

/* number of buckets in uop-flow-length histogram */
#define FLOW_HISTO_SIZE 9

/* VARIABLES/TYPES */

const char *core_commit_STM_t::commit_stall_str[CSTALL_num] = {
  "no stall                   ",
  "oldest inst not done       ",
  "oldest inst partially done ",
  "ROB is empty               ",
};

/*******************/
/* SETUP FUNCTIONS */
/*******************/

core_commit_STM_t::core_commit_STM_t(struct core_t * const arg_core):
  ROB_head(0), ROB_tail(0), ROB_num(0),
  when_rep_fetch_started(0), when_rep_fetched(0),
  when_rep_decode_started(0), when_rep_commit_started(0)
{
  struct core_knobs_t * knobs = arg_core->knobs;
  core = arg_core;
  ROB = (struct uop_t**) calloc(knobs->commit.ROB_size,sizeof(*ROB));
  if(!ROB)
    fatal("couldn't calloc ROB");
}

core_commit_STM_t::~core_commit_STM_t() {
    free(ROB);
}

void core_commit_STM_t::reg_stats(xiosim::stats::StatsDatabase* sdb) {
    int coreID = core->id;

    stat_reg_note(sdb, "\n#### COMMIT STATS ####");

    auto& sim_cycle_st = *stat_find_core_stat<tick_t>(sdb, coreID, "sim_cycle");
    auto& commit_insn_st = *stat_find_core_stat<counter_t>(sdb, coreID, "commit_insn");
    auto& commit_uops_st = *stat_find_core_stat<counter_t>(sdb, coreID, "commit_uops");

    stat_reg_core_formula(sdb, true, coreID, "commit_IPC", "IPC at commit",
                          commit_insn_st / sim_cycle_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "commit_uPC", "uPC at commit",
                          commit_uops_st / sim_cycle_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "avg_commit_flowlen",
                          "uops per instruction at commit", commit_uops_st / commit_insn_st, NULL);

    reg_core_queue_occupancy_stats(sdb, coreID, "ROB", &core->stat.ROB_occupancy,
                                   &core->stat.ROB_empty_cycles,
                                   &core->stat.ROB_full_cycles);

    core->stat.commit_stall = stat_reg_core_dist(
            sdb, coreID, "commit_stall", "breakdown of stalls at commit", 0, CSTALL_num,
            (PF_COUNT | PF_PDF), NULL, commit_stall_str, true, NULL);

    stat_reg_note(sdb, "#### TIMING STATS ####");
    /* instruction distribution stats */
    stat_reg_note(sdb, "\n#### INSTRUCTION STATS (no wrong-path) ####");
    stat_reg_core_formula(sdb, true, coreID, "num_insn", "total number of instructions committed",
                          commit_insn_st, NULL);
    auto& num_refs_st = stat_reg_core_counter(sdb, true, coreID, "num_refs",
                                              "total number of loads and stores committed",
                                              &core->stat.commit_refs, 0, true, NULL);
    auto& num_loads_st = stat_reg_core_counter(sdb, true, coreID, "num_loads",
                                               "total number of loads committed",
                                               &core->stat.commit_loads, 0, true, NULL);
    stat_reg_core_formula(sdb, true, coreID, "num_stores", "total number of stores committed",
                          num_refs_st - num_loads_st, "%12.0f");
}

void core_commit_STM_t::update_occupancy(void)
{
    /* ROB */
  core->stat.ROB_occupancy += ROB_num;
  if(ROB_num >= core->knobs->commit.ROB_size)
    core->stat.ROB_full_cycles++;
  if(ROB_num <= 0)
    core->stat.ROB_empty_cycles++;
}


/*************************/
/* MAIN COMMIT FUNCTIONS */
/*************************/

/* In-order instruction commit.  Individual uops cannot commit
   until it is guaranteed that the entire Mop's worth of uops will
   commit. */
void core_commit_STM_t::step(void)
{
  struct core_knobs_t * knobs = core->knobs;
  int commit_count = 0;
  enum commit_stall_t stall_reason = CSTALL_NONE;

  /* This is just a deadlock watchdog. If something got messed up
     in the pipeline and no forward progress is being made, this
     code will eventually detect it. A global watchdog will check
     if any core is making progress and accordingly if not.*/
  if(core->active && ((core->sim_cycle - core->exec->last_completed) > deadlock_threshold))
  {
    deadlocked = true; 
#ifdef ZTRACE
    ztrace_print(core->id, "Possible deadlock detected.");
#endif
    return;
  }

  /* MAIN COMMIT LOOP */
  for(commit_count=0;commit_count<knobs->commit.width;commit_count++)
  {
    if(ROB_num <= 0) /* nothing to commit */
    {
      stall_reason = commit_count?CSTALL_NONE:CSTALL_EMPTY;
      break;
    }

    struct Mop_t * Mop = ROB[ROB_head]->Mop;

    if(Mop->oracle.spec_mode)
      fatal("oldest instruction in processor is on wrong-path");

    /* Are all uops in the Mop completed? */
    if(Mop->commit.complete_index != -1) /* still some outstanding insts */
    {
      while(Mop->uop[Mop->commit.complete_index].timing.when_completed <= core->sim_cycle)
      {
        struct uop_t * uop = &Mop->uop[Mop->commit.complete_index];

        Mop->commit.complete_index += uop->decode.has_imm ? 3 : 1;
        if(Mop->commit.complete_index >= (int) Mop->decode.flow_length)
        {
          Mop->commit.complete_index = -1; /* Mark this Mop as all done */
          if(Mop->fetch.bpred_update)
          {
            core->fetch->bpred->update(Mop->fetch.bpred_update, Mop->decode.opflags,
                Mop->fetch.PC, Mop->fetch.ftPC, Mop->decode.targetPC, Mop->oracle.NextPC, Mop->oracle.taken_branch);
            core->fetch->bpred->return_state_cache(Mop->fetch.bpred_update);
            Mop->fetch.bpred_update = NULL;
          }
          break;
        }
      }
    }

    if(Mop->commit.complete_index == -1) /* commit the uops if the Mop is done */
    {
      struct uop_t * uop = ROB[ROB_head];
      zesto_assert(uop->timing.when_completed <= core->sim_cycle,(void)0);
      zesto_assert(uop->alloc.ROB_index == ROB_head,(void)0);
      zesto_assert(uop == &Mop->uop[Mop->commit.commit_index],(void)0);

      if(uop->decode.BOM && (uop->Mop->timing.when_commit_started == TICK_T_MAX))
        uop->Mop->timing.when_commit_started = core->sim_cycle;

      if(uop->decode.is_load)
        core->exec->LDQ_deallocate(uop);
      else if(uop->decode.is_sta)
        core->exec->STQ_deallocate_sta();
      else if(uop->decode.is_std) /* we alloc on STA, dealloc on STD */
      {
        if(!core->exec->STQ_deallocate_std(uop))
          break;
      }

      /* any remaining transactions in-flight (only for loads)
         should now be ignored - such load requests may exist, for
         example as a result of a load that completes early due to
         a hit in the STQ while the cache request is still making
         its way through the memory hierarchy. */
      if(uop->decode.is_load)
        uop->exec.action_id = core->new_action_id();

      if(uop->decode.EOM)
        uop->Mop->timing.when_commit_finished = core->sim_cycle;

      /* remove uop from ROB */
      ROB[ROB_head] = NULL;
      ROB_num --;
      ROB_head = modinc(ROB_head,knobs->commit.ROB_size); //(ROB_head+1) % knobs->commit.ROB_size;
      uop->alloc.ROB_index = -1;

      /* this cleans up idep/odep ptrs, register mappings, and
         commit stores to the real (non-spec) memory system */
      core->oracle->commit_uop(uop);

      /* mark uop as committed in Mop */
      Mop->commit.commit_index += uop->decode.has_imm ? 3 : 1;

      if(Mop->commit.commit_index >= (int) Mop->decode.flow_length)
      {
        Mop->commit.commit_index = -1; /* The entire Mop has been committed */

        /* Update stats */
        if(Mop->uop[Mop->decode.last_uop_index].decode.EOM)
        {
          ZESTO_STAT(core->stat.commit_insn++;)
        }

        ZESTO_STAT(core->stat.commit_uops += Mop->stat.num_uops;)
        ZESTO_STAT(core->stat.commit_refs += Mop->stat.num_refs;)
        ZESTO_STAT(core->stat.commit_loads += Mop->stat.num_loads;)

        core->update_stopwatch(Mop);

        /* Let the oracle know that we are done with this Mop. */
        core->oracle->commit(Mop);
      }

    }
    else
    {
      if(Mop->commit.complete_index == 0)
        stall_reason = CSTALL_NOT_READY;
      else
        stall_reason = CSTALL_PARTIAL;
      break; /* oldest Mop not done yet */
    }
  }

  ZESTO_STAT(stat_add_sample(core->stat.commit_stall, (int)stall_reason);)
}

void core_commit_STM_t::IO_step()
{
  /* Compatibility: Simulation can call this */
}

/* Walk ROB from youngest uop until we find the requested Mop.
   (NOTE: We stop at any uop belonging to the Mop.  We assume
   that recovery only occurs on Mop boundaries.)
   Release resources (PREGs, RS/ROB/LSQ entries, etc. as we go).
   If Mop == NULL, we're blowing away the entire pipeline. */
void
core_commit_STM_t::recover(const struct Mop_t * const Mop)
{
  assert(Mop != NULL);
  struct core_knobs_t * knobs = core->knobs;
  if(ROB_num > 0)
  {
    /* requested uop should always be in the ROB */
    int index = moddec(ROB_tail,knobs->commit.ROB_size); //(ROB_tail-1+knobs->commit.ROB_size) % knobs->commit.ROB_size;

    /* if there's only the one inst in the pipe, then we don't need to drain */
    if(knobs->alloc.drain_flush && (ROB[index]->Mop != Mop))
      core->alloc->start_drain();

    while(ROB[index] && (ROB[index]->Mop != Mop))
    {
      struct uop_t * dead_uop = ROB[index];

      zesto_assert(ROB_num > 0,(void)0);

      /* squash this instruction - this invalidates all in-flight actions (e.g., uop execution, cache accesses) */
      dead_uop->exec.action_id = core->new_action_id();

      /* update allocation scoreboard if appropriate */
      /* if(uop->alloc.RS_index != -1) */
      if(dead_uop->timing.when_issued == TICK_T_MAX && (dead_uop->alloc.port_assignment != -1))
        core->alloc->RS_deallocate(dead_uop);

      if(dead_uop->alloc.RS_index != -1) /* currently in RS */
        core->exec->RS_deallocate(dead_uop);


      /* In the following, we have to check it the uop has even been allocated yet... this has
         to do with our non-atomic implementation of allocation for fused-uops */
      if(dead_uop->decode.is_load)
      {
        if(dead_uop->timing.when_allocated != TICK_T_MAX)
          core->exec->LDQ_squash(dead_uop);
      }
      else if(dead_uop->decode.is_std) /* dealloc when we get to the STA */
      {
        if(dead_uop->timing.when_allocated != TICK_T_MAX)
          core->exec->STQ_squash_std(dead_uop);
      }
      else if(dead_uop->decode.is_sta)
      {
        if(dead_uop->timing.when_allocated != TICK_T_MAX)
          core->exec->STQ_squash_sta(dead_uop);
      }

      /* clean up idep/odep pointers.  Since we're working our
         way back from the most speculative instruction, we only
         need to clean-up our parent's forward-pointers (odeps)
         and our own back-pointers (our fwd-ptrs would have
         already cleaned-up our own children). */
      for(size_t i=0;i<MAX_IDEPS;i++)
      {
        struct uop_t * parent = dead_uop->exec.idep_uop[i];
        if(parent) /* I have an active parent */
        {
          struct odep_t * prev, * current;
          prev = NULL;
          current = parent->exec.odep_uop;
          while(current)
          {
            if((current->uop == dead_uop) && (current->op_num == (int)i))
              break;
            prev = current;
            current = current->next;
          }

          zesto_assert(current,(void)0);

          /* remove self from parent's odep list */
          if(prev)
            prev->next = current->next;
          else
            parent->exec.odep_uop = current->next;

          /* eliminate my own idep */
          dead_uop->exec.idep_uop[i] = NULL;

          /* return the odep struct */
          core->return_odep_link(current);
        }
      }

      zesto_assert(dead_uop->exec.odep_uop == NULL,(void)0);

      ROB[index] = NULL;
      ROB_tail = moddec(ROB_tail,knobs->commit.ROB_size); //(ROB_tail-1+knobs->commit.ROB_size) % knobs->commit.ROB_size;
      ROB_num --;
      zesto_assert(ROB_num >= 0,(void)0);

      index = moddec(index,knobs->commit.ROB_size); //(index-1+knobs->commit.ROB_size) % knobs->commit.ROB_size;
    }
  }
}

void
core_commit_STM_t::recover(void)
{
  struct core_knobs_t * knobs = core->knobs;
  if(ROB_num > 0)
  {
    /* requested uop should always be in the ROB */
    int index = moddec(ROB_tail,knobs->commit.ROB_size); //(ROB_tail-1+knobs->commit.ROB_size) % knobs->commit.ROB_size;

    while(ROB[index])
    {
      struct uop_t * dead_uop = ROB[index];

      zesto_assert(ROB_num > 0,(void)0);

      /* squash this instruction - this invalidates all in-flight actions (e.g., uop execution, cache accesses) */
      dead_uop->exec.action_id = core->new_action_id();

      /* update allocation scoreboard if appropriate */
      if(dead_uop->timing.when_issued == TICK_T_MAX && (dead_uop->alloc.port_assignment != -1))
        core->alloc->RS_deallocate(dead_uop);

      if(dead_uop->alloc.RS_index != -1) /* currently in RS */
        core->exec->RS_deallocate(dead_uop);


      /* In the following, we have to check it the uop has even
         been allocated yet... this has to do with our non-atomic
         implementation of allocation for fused-uops */
      if(dead_uop->decode.is_load)
      {
        if(dead_uop->timing.when_allocated != TICK_T_MAX)
          core->exec->LDQ_squash(dead_uop);
      }
      else if(dead_uop->decode.is_std) /* dealloc when we get to the STA */
      {
        if(dead_uop->timing.when_allocated != TICK_T_MAX)
          core->exec->STQ_squash_std(dead_uop);
      }
      else if(dead_uop->decode.is_sta)
      {
        if(dead_uop->timing.when_allocated != TICK_T_MAX)
          core->exec->STQ_squash_sta(dead_uop);
      }

      /* clean up idep/odep pointers.  Since we're working our
         way back from the most speculative instruction, we only
         need to clean-up our parent's forward-pointers (odeps)
         and our own back-pointers (our fwd-ptrs would have
         already cleaned-up our own children). */
      for(size_t i=0;i<MAX_IDEPS;i++)
      {
        struct uop_t * parent = dead_uop->exec.idep_uop[i];
        if(parent) /* I have an active parent */
        {
          struct odep_t * prev, * current;
          prev = NULL;
          current = parent->exec.odep_uop;
          while(current)
          {
            if((current->uop == dead_uop) && (current->op_num == (int)i))
              break;
            prev = current;
            current = current->next;
          }

          zesto_assert(current,(void)0);

          /* remove self from parent's odep list */
          if(prev)
            prev->next = current->next;
          else
            parent->exec.odep_uop = current->next;

          /* eliminate my own idep */
          dead_uop->exec.idep_uop[i] = NULL;

          /* return the odep struct */
          core->return_odep_link(current);
        }
      }

      zesto_assert(dead_uop->exec.odep_uop == NULL,(void)0);

      ROB[index] = NULL;
      ROB_tail = moddec(ROB_tail,knobs->commit.ROB_size); //(ROB_tail-1+knobs->commit.ROB_size) % knobs->commit.ROB_size;
      ROB_num --;
      zesto_assert(ROB_num >= 0,(void)0);

      index = moddec(index,knobs->commit.ROB_size); //(index-1+knobs->commit.ROB_size) % knobs->commit.ROB_size;
    }
  }

  core->exec->recover_check_assertions();
  zesto_assert(ROB_num == 0,(void)0);
}

bool core_commit_STM_t::ROB_available(void)
{
  struct core_knobs_t * knobs = core->knobs;
  return ROB_num < knobs->commit.ROB_size;
}

bool core_commit_STM_t::ROB_empty(void)
{
  return 0 == ROB_num;
}

bool core_commit_STM_t::pipe_empty(void)
{
  return 0 == ROB_num;
}

void core_commit_STM_t::ROB_insert(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  ROB[ROB_tail] = uop;
  uop->alloc.ROB_index = ROB_tail;
  ROB_num++;
  ROB_tail = modinc(ROB_tail,knobs->commit.ROB_size); //(ROB_tail+1) % knobs->commit.ROB_size;
}

void core_commit_STM_t::ROB_fuse_insert(struct uop_t * const uop)
{
  fatal("fusion not supported in STM commit module");
}

/* Dummy fucntions for compatibility with IO pipe */
void core_commit_STM_t::pre_commit_insert(struct uop_t * const uop)
{
  fatal("shouldn't be called");
}
void core_commit_STM_t::pre_commit_fused_insert(struct uop_t * const uop)
{
  fatal("shouldn't be called");
}
bool core_commit_STM_t::pre_commit_available()
{
  fatal("shouldn't be called");
}
void core_commit_STM_t::pre_commit_step()
{
  /* Compatibility: simulation can call this */
}
void core_commit_STM_t::pre_commit_recover(struct Mop_t * const Mop)
{
  fatal("shouldn't be called");
}
void core_commit_STM_t::squash_uop(struct uop_t * const uop)
{
  fatal("shouldn't be called");
}	

#endif
