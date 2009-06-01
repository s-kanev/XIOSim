/* commit-STM.cpp - Simple(r) Timing Model */
/*
 * __COPYRIGHT__ GT
 */

#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(commit_opt_string,"STM"))
    return new core_commit_STM_t(core);
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
  virtual void reg_stats(struct stat_sdb_t * const sdb);
  virtual void update_occupancy(void);

  virtual void step(void);
  virtual void recover(const struct Mop_t * const Mop);
  virtual void recover(void);

  virtual bool ROB_available(void);
  virtual bool ROB_empty(void);
  virtual void ROB_insert(struct uop_t * const uop);
  virtual void ROB_fuse_insert(struct uop_t * const uop);

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

void
core_commit_STM_t::reg_stats(struct stat_sdb_t * const sdb)
{
  char buf[1024];
  char buf2[1024];
  struct thread_t * arch = core->current_thread;

  stat_reg_note(sdb,"\n#### COMMIT STATS ####");

  sprintf(buf,"c%d.commit_insn",arch->id);
  stat_reg_counter(sdb, true, buf, "total number of instructions committed", &core->stat.commit_insn, core->stat.commit_insn, NULL);
  sprintf(buf,"c%d.commit_uops",arch->id);
  stat_reg_counter(sdb, true, buf, "total number of uops committed", &core->stat.commit_uops, core->stat.commit_uops, NULL);
  sprintf(buf,"c%d.commit_IPC",arch->id);
  sprintf(buf2,"c%d.commit_insn/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "IPC at commit", buf2, NULL);
  sprintf(buf,"c%d.commit_uPC",arch->id);
  sprintf(buf2,"c%d.commit_uops/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "uPC at commit", buf2, NULL);
  sprintf(buf,"c%d.avg_commit_flowlen",arch->id);
  sprintf(buf2,"c%d.commit_uops/c%d.commit_insn",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "uops per instruction at commit", buf2, NULL);

  sprintf(buf,"c%d.commit_dead_lock_flushes",arch->id);
  stat_reg_counter(sdb, true, buf, "total number of pipe-flushes due to dead-locked pipeline", &core->stat.commit_deadlock_flushes, core->stat.commit_deadlock_flushes, NULL);
  sprintf(buf,"c%d.ROB_occupancy",arch->id);
  stat_reg_counter(sdb, false, buf, "total ROB occupancy", &core->stat.ROB_occupancy, core->stat.ROB_occupancy, NULL);
  sprintf(buf,"c%d.ROB_empty",arch->id);
  stat_reg_counter(sdb, false, buf, "total cycles ROB was empty", &core->stat.ROB_empty_cycles, core->stat.ROB_empty_cycles, NULL);
  sprintf(buf,"c%d.ROB_full",arch->id);
  stat_reg_counter(sdb, false, buf, "total cycles ROB was full", &core->stat.ROB_full_cycles, core->stat.ROB_full_cycles, NULL);
  sprintf(buf,"c%d.ROB_avg",arch->id);
  sprintf(buf2,"c%d.ROB_occupancy/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "average ROB occupancy", buf2, NULL);
  sprintf(buf,"c%d.ROB_frac_empty",arch->id);
  sprintf(buf2,"c%d.ROB_empty/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "fraction of cycles ROB was empty", buf2, NULL);
  sprintf(buf,"c%d.ROB_frac_full",arch->id);
  sprintf(buf2,"c%d.ROB_full/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "fraction of cycles ROB was full", buf2, NULL);

  sprintf(buf,"c%d.commit_stall",core->current_thread->id);
  core->stat.commit_stall = stat_reg_dist(sdb, buf,
                                           "breakdown of stalls at commit",
                                           /* initial value */0,
                                           /* array size */CSTALL_num,
                                           /* bucket size */1,
                                           /* print format */(PF_COUNT|PF_PDF),
                                           /* format */NULL,
                                           /* index map */commit_stall_str,
                                           /* print fn */NULL);

  stat_reg_note(sdb,"#### TIMING STATS ####");
  sprintf(buf,"c%d.sim_cycle",arch->id);
  stat_reg_qword(sdb, true, buf, "total number of cycles when last instruction (or uop) committed", (qword_t*) &core->stat.final_sim_cycle, core->stat.final_sim_cycle, NULL);
  /* instruction distribution stats */
  stat_reg_note(sdb,"\n#### INSTRUCTION STATS (no wrong-path) ####");
  sprintf(buf,"c%d.num_insn",arch->id);
  stat_reg_counter(sdb, true, buf, "total number of instructions committed", &core->stat.commit_insn, core->stat.commit_insn, NULL);
  sprintf(buf,"c%d.num_refs",arch->id);
  stat_reg_counter(sdb, true, buf, "total number of loads and stores committed", &core->stat.commit_refs, core->stat.commit_refs, NULL);
  sprintf(buf,"c%d.num_loads",arch->id);
  stat_reg_counter(sdb, true, buf, "total number of loads committed", &core->stat.commit_loads, core->stat.commit_loads, NULL);
  sprintf(buf2,"c%d.num_refs - c%d.num_loads",arch->id,arch->id);
  sprintf(buf,"c%d.num_stores",arch->id);
  stat_reg_formula(sdb, true, buf, "total number of stores committed", buf2, "%12.0f");
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

  /* This is just a deadlock watchdog.  If something got messed up
     in the pipeline and no forward progress is being made, this
     code will eventually detect it and flush the pipeline in an
     attempt to un-wedge the processor.  If the processor then
     deadlocks again without having first made any more forward
     progress, we give up and kill the simulator. */
  if((sim_cycle - core->exec->last_completed) > deadlock_threshold)
  {
    if(core->exec->last_completed_count == core->stat.eio_commit_insn)
    {
      char buf[256];
      snprintf(buf,sizeof(buf),"At cycle %lld, core[%d] has not completed a uop in %d cycles... definite deadlock",(long long)sim_cycle,core->current_thread->id,deadlock_threshold);
      zesto_fatal(buf,(void)0);
    }
    else
    {
      warn("At cycle %lld, core[%d] has not completed a uop in %d cycles... possible deadlock, flushing pipeline",(long long)sim_cycle,core->current_thread->id,deadlock_threshold);

      /* flush the entire pipeline, correct path or not... passing
         NULL's causes the recover functions to throw everything
         away. */
      core->oracle->complete_flush();
      /*core->commit->*/recover();
      core->exec->recover();
      core->alloc->recover();
      core->decode->recover();
      core->fetch->recover(core->current_thread->regs.regs_NPC);
      ZESTO_STAT(stat_add_sample(core->stat.commit_stall, (int)CSTALL_EMPTY);)
      ZESTO_STAT(core->stat.commit_deadlock_flushes++;)
      core->exec->last_completed = sim_cycle; /* so we don't do this again next cycle */
      core->exec->last_completed_count = core->stat.eio_commit_insn;
    }
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
      zesto_fatal("oldest instruction in processor is on wrong-path",(void)0);

    /* Are all uops in the Mop completed? */
    if(Mop->commit.complete_index != -1) /* still some outstanding insts */
    {
      while(Mop->uop[Mop->commit.complete_index].timing.when_completed <= sim_cycle)
      {
        struct uop_t * uop = &Mop->uop[Mop->commit.complete_index];

        Mop->commit.complete_index += uop->decode.has_imm ? 3 : 1;
        if(Mop->commit.complete_index >= Mop->decode.flow_length)
        {
          Mop->commit.complete_index = -1; /* Mark this Mop as all done */
          if(Mop->fetch.bpred_update)
          {
            core->fetch->bpred->update(Mop->fetch.bpred_update,Mop->decode.opflags,
                Mop->fetch.PC, Mop->decode.targetPC, Mop->oracle.NextPC, (Mop->oracle.NextPC != (Mop->fetch.PC + Mop->fetch.inst.len)));
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
      zesto_assert(uop->timing.when_completed <= sim_cycle,(void)0);
      zesto_assert(uop->alloc.ROB_index == ROB_head,(void)0);
      zesto_assert(uop == &Mop->uop[Mop->commit.commit_index],(void)0);

      if(uop->decode.BOM && (uop->Mop->timing.when_commit_started == TICK_T_MAX))
        uop->Mop->timing.when_commit_started = sim_cycle;

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
        uop->Mop->timing.when_commit_finished = sim_cycle;

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

      if(Mop->commit.commit_index >= Mop->decode.flow_length)
      {
        Mop->commit.commit_index = -1; /* The entire Mop has been committed */

        /* Update stats */
        if(Mop->uop[Mop->decode.last_uop_index].decode.EOM)
        {
          core->stat.eio_commit_insn++;
          total_commit_insn ++;
          ZESTO_STAT(core->stat.commit_insn++;)
        }

        total_commit_uops += Mop->stat.num_uops;
        ZESTO_STAT(core->stat.commit_uops += Mop->stat.num_uops;)
        ZESTO_STAT(core->stat.commit_refs += Mop->stat.num_refs;)
        ZESTO_STAT(core->stat.commit_loads += Mop->stat.num_loads;)

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

    /*****************/
    /* finish early? */
    /*****************/
    if(core->current_thread->active)
    {
      if ( ( max_cycles && sim_cycle >= max_cycles ) ||
           (max_insts && core->stat.commit_insn >= max_insts) ||
           (max_uops && core->stat.commit_uops >= max_uops)  )
      {
        core->stat.final_sim_cycle = sim_cycle; /* make note of when this core stopped simulating */
        if(max_cycles && sim_cycle >= max_cycles)
          fprintf(stderr,"# Simulation cycle ");
        else if(max_insts && max_uops)
          fprintf(stderr,"# Committed instruction/uop ");
        else if(max_insts)
          fprintf(stderr,"# Committed instruction ");
        else
          fprintf(stderr,"# Committed uop ");
        fprintf(stderr,"limit reached for core %d.\n",core->current_thread->id);

        simulated_processes_remaining--;
        core->current_thread->active = false;
        core->fetch->bpred->freeze_stats();
        core->exec->freeze_stats();
        cache_freeze_stats(core);
        /* start this core over */

        if(simulated_processes_remaining <= 0)
          longjmp(sim_exit_buf, /* exitcode + fudge */0 + 1);
      }
    }

    /* Reset the trace (eio file input) if we've hit the end of the
       trace.  This is used in multi-core simulation mode to keep
       cores that have reached their simulation limits busy. */
    if (trace_limit && (core->stat.eio_commit_insn >= trace_limit))
    {
      core->stat.eio_commit_insn = 0;
      core->oracle->reset_execution();
    }
  }

  ZESTO_STAT(stat_add_sample(core->stat.commit_stall, (int)stall_reason);)
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
      int i;
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
      for(i=0;i<MAX_IDEPS;i++)
      {
        struct uop_t * parent = dead_uop->exec.idep_uop[i];
        if(parent) /* I have an active parent */
        {
          struct odep_t * prev, * current;
          prev = NULL;
          current = parent->exec.odep_uop;
          while(current)
          {
            if((current->uop == dead_uop) && (current->op_num == i))
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
      int i;
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
      for(i=0;i<MAX_IDEPS;i++)
      {
        struct uop_t * parent = dead_uop->exec.idep_uop[i];
        if(parent) /* I have an active parent */
        {
          struct odep_t * prev, * current;
          prev = NULL;
          current = parent->exec.odep_uop;
          while(current)
          {
            if((current->uop == dead_uop) && (current->op_num == i))
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

#endif
