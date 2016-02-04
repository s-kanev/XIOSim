/* commit-DPM.cpp - Detailed Pipeline Model */
/*
 * __COPYRIGHT__ GT
 */

#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(commit_opt_string,"DPM"))
    return new core_commit_DPM_t(core);
#else

class core_commit_DPM_t:public core_commit_t
{
  enum commit_stall_t {CSTALL_NONE,      /* no stall */
                       CSTALL_NOT_READY, /* oldest inst not done (no uops finished) */
                       CSTALL_PARTIAL,   /* oldest inst not done (but some uops finished) */
                       CSTALL_EMPTY,     /* ROB is empty, nothing to commit */
                       CSTALL_JECLEAR_INFLIGHT, /* Mop is done, but its jeclear hasn't been handled yet */
                       CSTALL_MAX_BRANCHES, /* exceeded maximum number of branches committed per cycle */
                       CSTALL_STQ, /* Store can't deallocate from STQ */
                       CSTALL_num
                     };

  public:

  core_commit_DPM_t(struct core_t * const core);
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
  virtual int squash_uop(struct uop_t * const uop);


  protected:

  struct uop_t ** ROB;
  int ROB_head;
  int ROB_tail;
  int ROB_num;
  int ROB_eff_num;

  static const char *commit_stall_str[CSTALL_num];

  /* additional temps to track timing of REP insts */
  tick_t when_rep_fetch_started;
  tick_t when_rep_fetched;
  tick_t when_rep_decode_started;
  tick_t when_rep_commit_started;

  tick_t ticker;
};

/* number of buckets in uop-flow-length histogram */
#define FLOW_HISTO_SIZE 9

/* VARIABLES/TYPES */

const char *core_commit_DPM_t::commit_stall_str[CSTALL_num] = {
  "no stall                   ",
  "oldest inst not done       ",
  "oldest inst partially done ",
  "ROB is empty               ",
  "Mop done, jeclear in flight",
  "branch commit limit        ",
  "store can't deallocate     "
};

/*******************/
/* SETUP FUNCTIONS */
/*******************/

core_commit_DPM_t::core_commit_DPM_t(struct core_t * const arg_core):
  ROB_head(0), ROB_tail(0), ROB_num(0), ROB_eff_num(0),
  when_rep_fetch_started(0), when_rep_fetched(0),
  when_rep_decode_started(0), when_rep_commit_started(0)
{
  struct core_knobs_t * knobs = arg_core->knobs;
  core = arg_core;
  ROB = (struct uop_t**) calloc(knobs->commit.ROB_size,sizeof(*ROB));
  if(!ROB)
    fatal("couldn't calloc ROB");
}

void core_commit_DPM_t::reg_stats(xiosim::stats::StatsDatabase* sdb) {
    int coreID = core->id;

    stat_reg_note(sdb, "\n#### COMMIT STATS ####");

    auto& sim_cycle_st = *stat_find_core_stat<tick_t>(sdb, coreID, "sim_cycle");
    auto& commit_insn_st = *stat_find_core_stat<counter_t>(sdb, coreID, "commit_insn");
    auto& commit_uops_st = *stat_find_core_stat<counter_t>(sdb, coreID, "commit_uops");
    auto& commit_eff_uops_st = *stat_find_core_stat<counter_t>(sdb, coreID, "commit_eff_uops");

    auto& commit_bytes_st = stat_reg_core_counter(sdb, true, coreID, "commit_bytes",
                                                  "total number of bytes committed",
                                                  &core->stat.commit_bytes, 0, true, NULL);
    stat_reg_core_formula(sdb, true, coreID, "commit_BPC", "BPC (bytes per cycle) at commit",
                          commit_bytes_st / sim_cycle_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "commit_IPC", "IPC at commit",
                          commit_insn_st / sim_cycle_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "commit_uPC", "uPC at commit",
                          commit_uops_st / sim_cycle_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "commit_euPC", "effective uPC at commit",
                          commit_eff_uops_st / sim_cycle_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "commit_byte_per_inst",
                          "average bytes per instruction", commit_bytes_st / commit_insn_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "commit_byte_per_uops", "average bytes per uop",
                          commit_bytes_st / commit_uops_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "commit_byte_per_eff_uop",
                          "average bytes per effective uop", commit_bytes_st / commit_eff_uops_st,
                          NULL);
    stat_reg_core_formula(sdb, true, coreID, "avg_commit_flowlen",
                          "uops per instruction at commit", commit_uops_st / commit_insn_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "avg_commit_eff_flowlen",
                          "effective uops per instruction at commit",
                          commit_eff_uops_st / commit_insn_st, NULL);

    auto& commit_fusions_st = stat_reg_core_counter(sdb, true, coreID, "commit_fusions",
                                                    "total number of fused uops committed",
                                                    &core->stat.commit_fusions, 0, true, NULL);
    stat_reg_core_formula(sdb, true, coreID, "commit_fusion_uops", "fused uops at commit",
                          (commit_eff_uops_st - commit_uops_st) + commit_fusions_st, "%12.0f");
    // TODO: Duplicating the formula terms for now.
    stat_reg_core_formula(
            sdb, true, coreID, "commit_frac_fusion_uops",
            "fraction of effective uops fused at commit",
            ((commit_eff_uops_st - commit_uops_st) + commit_fusions_st) / commit_eff_uops_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "commit_fusion_compression",
                          "fraction of effective uops compressed via fusion at commit",
                          (commit_eff_uops_st - commit_uops_st) / commit_eff_uops_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "commit_fusion_expansion",
                          "average number of effective uops per uop (fused or standalone) commit",
                          commit_eff_uops_st / commit_uops_st, NULL);

    reg_core_queue_occupancy_stats(sdb, coreID, "ROB", &core->stat.ROB_occupancy,
                                   &core->stat.ROB_empty_cycles,
                                   &core->stat.ROB_full_cycles);

    auto store_lookups_st = stat_find_core_stat<counter_t>(sdb, coreID, "DL1.store_lookups");
    auto& split_accesses_st =
            stat_reg_core_counter(sdb, true, coreID, "DL1_store_split_accesses",
                                  "number of stores requiring split accesses",
                                  &core->stat.DL1_store_split_accesses, 0, true, NULL);
    stat_reg_core_formula(sdb, true, coreID, "DL1_store_split_frac",
                          "fraction of stores requiring split accesses",
                          split_accesses_st / (*store_lookups_st - split_accesses_st), NULL);

    core->stat.commit_stall = stat_reg_core_dist(
            sdb, coreID, "commit_stall", "breakdown of stalls at commit", 0, CSTALL_num, 1,
            (PF_COUNT | PF_PDF), NULL, commit_stall_str, true, NULL);

    stat_reg_note(sdb, "#### TIMING STATS ####");
    /* cumulative slip cycles (not printed) */
    auto& fetch_Tslip = stat_reg_core_counter(sdb, false, coreID, "Mop_fetch_Tslip",
                                            "total Mop fetch slip cycles",
                                            &core->stat.Mop_fetch_slip, 0, true, NULL);
    auto& f2d_Tslip = stat_reg_core_counter(
            sdb, false, coreID, "Mop_f2d_Tslip", "total Mop fetch-to-decode slip cycles",
            &core->stat.Mop_fetch2decode_slip, 0, true, NULL);
    auto& decode_Tslip = stat_reg_core_counter(sdb, false, coreID, "Mop_decode_Tslip",
                                             "total Mop decode slip cycles",
                                             &core->stat.Mop_decode_slip, 0, true, NULL);
    auto& d2a_Tslip = stat_reg_core_counter(
            sdb, false, coreID, "uop_d2a_Tslip", "total uop decode-to-alloc slip cycles",
            &core->stat.uop_decode2alloc_slip, 0, true, NULL);
    auto& a2r_Tslip = stat_reg_core_counter(
            sdb, false, coreID, "uop_a2r_Tslip", "total uop alloc-to-ready slip cycles",
            &core->stat.uop_alloc2ready_slip, 0, true, NULL);
    auto& r2i_Tslip = stat_reg_core_counter(
            sdb, false, coreID, "uop_r2i_Tslip", "total uop ready-to-issue slip cycles",
            &core->stat.uop_ready2issue_slip, 0, true, NULL);
    auto& i2e_Tslip = stat_reg_core_counter(sdb, false, coreID, "uop_i2e_Tslip",
                                          "total uop issue-to-exec slip cycles",
                                          &core->stat.uop_issue2exec_slip, 0, true, NULL);
    auto& e2w_Tslip = stat_reg_core_counter(
            sdb, false, coreID, "uop_e2w_Tslip", "total uop exec-to-WB slip cycles",
            &core->stat.uop_exec2complete_slip, 0, true, NULL);
    auto& w2c_Tslip = stat_reg_core_counter(
            sdb, false, coreID, "uop_w2c_Tslip", "total uop WB-to-commit slip cycles",
            &core->stat.uop_complete2commit_slip, 0, true, NULL);
    auto& d2c_Tslip = stat_reg_core_counter(
            sdb, false, coreID, "Mop_d2c_Tslip", "total Mop decode-to-commit slip cycles",
            &core->stat.Mop_decode2commit_slip, 0, true, NULL);
    auto& commit_Tslip = stat_reg_core_counter(sdb, false, coreID, "Mop_commit_Tslip",
                                             "total Mop commit slip cycles",
                                             &core->stat.Mop_commit_slip, 0, true, NULL);
    stat_reg_core_counter(sdb, true, coreID, "num_traps",
                          "total number of traps committed",
                          &core->stat.commit_traps, 0, true, NULL);
    /* average slip cycles */
    stat_reg_core_formula(sdb, true, coreID, "Mop_fetch_avg_slip", "Mop fetch average delay",
                          fetch_Tslip / commit_insn_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "Mop_f2d_avg_slip",
                          "Mop fetch-to-decode average delay",
                          f2d_Tslip / commit_insn_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "Mop_decode_avg_slip", "Mop decode average delay",
                          decode_Tslip / commit_insn_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "uop_d2a_avg_slip",
                          "uop decode-to-alloc average delay", d2a_Tslip / commit_uops_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "uop2_a2r_avg_slip",
                          "uop alloc-to-ready average delay", a2r_Tslip / commit_uops_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "uop_r2i_avg_slip",
                          "uop ready-to-issue average delay", r2i_Tslip / commit_uops_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "uop_i2e_avg_slip",
                          "uop issue-to-exec average delay", i2e_Tslip / commit_uops_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "uop_e2w_avg_slip", "uop exec-to-WB average delay",
                          e2w_Tslip / commit_uops_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "uop_w2c_avg_slip", "uop WB-to-commit average delay",
                          w2c_Tslip / commit_uops_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "Mop_d2c_avg_slip",
                          "Mop decode-to-commit average delay",
                          d2c_Tslip / commit_insn_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "Mop_commit_avg_slip", "Mop commit average delay",
                          commit_Tslip / commit_insn_st, NULL);

    // TODO: Support sum of other formulas. For now, just write the whole thing out by hand.
    char stat_name[1024];
    sprintf(stat_name, "c%d.Mop_avg_end_to_end", coreID);
    Formula Mop_avg_end_to_end(stat_name, "Mop average end-to-end pipeline delay");
    Mop_avg_end_to_end =
            fetch_Tslip / commit_insn_st  +    // Mop_fetch_avg_slip.
            f2d_Tslip / commit_insn_st    +    // Mop_f2d_avg_slip.
            decode_Tslip / commit_insn_st +    // Mop_d2c_avg_slip.
            commit_Tslip / commit_insn_st;     // Mop_commit_avg_slip.

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
    stat_reg_core_counter(sdb, true, coreID, "num_branches", "total number of branches committed",
                          &core->stat.commit_branches, 0, true, NULL);
    auto& num_rep_insn_st = stat_reg_core_counter(sdb, true, coreID, "num_rep_insn",
                                                  "total number of REP insts committed",
                                                  &core->stat.commit_rep_insn, 0, true, NULL);
    auto& num_rep_iter_st = stat_reg_core_counter(sdb, true, coreID, "num_rep_iter",
                                                  "total number of REP iterations committed",
                                                  &core->stat.commit_rep_iterations, 0, true, NULL);
    auto& num_rep_uops_st = stat_reg_core_counter(sdb, true, coreID, "num_rep_uops",
                                                  "total number of uops in REP insts committed",
                                                  &core->stat.commit_rep_uops, 0, true, NULL);
    stat_reg_core_formula(sdb, true, coreID, "num_avg_reps", "average iterations per REP inst",
                          num_rep_iter_st / num_rep_insn_st, "%12.2f");
    stat_reg_core_formula(sdb, true, coreID, "num_avg_rep_uops", "average uops per REP inst",
                          num_rep_uops_st / num_rep_insn_st, "%12.2f");
    auto& num_UROM_insn_st = stat_reg_core_counter(sdb, true, coreID, "num_UROM_insn",
                                                   "total number of insn using the UROM committed",
                                                   &core->stat.commit_UROM_insn, 0, true, NULL);
    auto& num_UROM_uops_st = stat_reg_core_counter(sdb, true, coreID, "num_UROM_uops",
                                                   "total number of uops using the UROM committed",
                                                   &core->stat.commit_UROM_uops, 0, true, NULL);
    stat_reg_core_counter(sdb, true, coreID, "num_UROM_eff_uops",
                          "total number of effective uops using the UROM committed",
                          &core->stat.commit_UROM_eff_uops, 0, true, NULL);
    stat_reg_core_formula(sdb, true, coreID, "num_avg_UROM_uops", "average uops per UROM inst",
                          num_UROM_uops_st / num_UROM_insn_st, "%12.2f");
    stat_reg_core_formula(sdb, true, coreID, "avg_flowlen", "average uops per instruction",
                          commit_uops_st / commit_insn_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "avg_eff_flowlen",
                          "average effective uops per instruction",
                          commit_eff_uops_st / commit_insn_st, NULL);
    stat_reg_core_counter(sdb, true, coreID, "regfile_writes", "number of register file writes",
                          &core->stat.regfile_writes, 0, true, NULL);
    stat_reg_core_counter(sdb, true, coreID, "fp_regfile_writes",
                          "number of fp register file writes", &core->stat.fp_regfile_writes, 0,
                          true, NULL);
    core->stat.flow_histo =
            stat_reg_core_dist(sdb, coreID, "flow_lengths", "histogram of uop flow lengths", 0,
                               FLOW_HISTO_SIZE, 1, (PF_COUNT | PF_PDF), NULL, NULL, true, NULL);
    core->stat.eff_flow_histo = stat_reg_core_dist(
            sdb, coreID, "eff_flow_lengths", "histogram of effective uop flow lengths", 0,
            FLOW_HISTO_SIZE, 1, (PF_COUNT | PF_PDF), NULL, NULL, true, NULL);
}

void core_commit_DPM_t::update_occupancy(void)
{
    /* ROB */
  core->stat.ROB_occupancy += ROB_num;
  core->stat.ROB_eff_occupancy += ROB_eff_num;
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
void core_commit_DPM_t::step(void)
{
  struct core_knobs_t * knobs = core->knobs;
  int commit_count = 0;
  enum commit_stall_t stall_reason = CSTALL_NONE;
  int branches_committed = 0;

  /* This is just a deadlock watchdog. If something got messed up
     in the pipeline and no forward progress is being made, this
     code will eventually detect it. A global watchdog will check
     if any core is making progress and accordingly if not.*/
  if(core->active && ((core->sim_cycle - core->exec->last_completed) > deadlock_threshold))
  {
    deadlocked = true;
    return;
  }

  /* deallocate at most commit_width stores from the (senior) STQ per cycle */
  for(int STQ_commit_count = 0; STQ_commit_count < knobs->commit.width; STQ_commit_count++)
    core->exec->STQ_deallocate_senior();

  /* MAIN COMMIT LOOP */
  for(commit_count=0;commit_count<knobs->commit.width;commit_count++)
  {
    if(ROB_num <= 0) /* nothing to commit */
    {
      stall_reason = CSTALL_EMPTY;
      break;
    }

    struct Mop_t * Mop = ROB[ROB_head]->Mop;

    /* For branches, don't commit until the corresponding jeclear
       (if any) has been processed by the front-end. */
    if(Mop->commit.jeclear_in_flight)
    {
      stall_reason = CSTALL_JECLEAR_INFLIGHT;
      break;
    }

    if(Mop->decode.is_ctrl && knobs->commit.branch_limit && (branches_committed >= knobs->commit.branch_limit))
    {
      stall_reason = CSTALL_MAX_BRANCHES;
      break;
    }

    zesto_assert(!Mop->oracle.spec_mode, (void)0);

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
#ifdef ZTRACE
          ztrace_print(Mop,"c|complete|all uops completed execution");
#endif
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

      if(uop->decode.is_load || uop->decode.is_fence)
        core->exec->LDQ_deallocate(uop);
      else if(uop->decode.is_sta)
        core->exec->STQ_deallocate_sta();
      else if(uop->decode.is_std) /* we alloc on STA, dealloc on STD */
      {
        if(!core->exec->STQ_deallocate_std(uop)) {
          stall_reason = CSTALL_STQ;
          break;
        }
      }

      /* any remaining transactions in-flight (only for loads)
         should now be ignored - such load requests may exist, for
         example as a result of a load that completes early due to
         a hit in the STQ while the cache request is still making
         its way through the memory hierarchy. */
      if(uop->decode.is_load)
        uop->exec.action_id = core->new_action_id();

#ifdef ZTRACE
      ztrace_print(uop,"c|commit|uop committed");
#endif

      if(uop->decode.EOM)
        uop->Mop->timing.when_commit_finished = core->sim_cycle;

      /* remove uop from ROB */
      if((!uop->decode.in_fusion) || (uop->decode.fusion_next == NULL)) /* fusion dealloc's on fusion-tail */
      {
        ROB[ROB_head] = NULL;
        ROB_num --;
        ROB_eff_num --;
        ROB_head = modinc(ROB_head,knobs->commit.ROB_size); //(ROB_head+1) % knobs->commit.ROB_size;
        if(uop->decode.in_fusion)
        {
          ZESTO_STAT(core->stat.commit_fusions++;)
        }
      }
      else /* fusion body doesn't count toward commit width */
      {
        commit_count--;
        /* XXX: this is really ugly.  To avoid inserting another
           uop-traversing loop within the commit loop, we replace
           the ROB[.] pointer with the next uop to commit (for
           fusion).  That is, if we have a two-uop fused set (A,B),
           originally ROB[.] would point to A, which has a pointer
           through fusion_next to B.  Once we commit A, we will
           replace ROB[.] with B. */
        ROB[ROB_head] = uop->decode.fusion_next;
        ROB_eff_num --;
        zesto_assert(ROB_eff_num >= 0,(void)0);
      }
      uop->alloc.ROB_index = -1;

      if(!uop->Mop->decode.is_trap)
      {
        ZESTO_STAT(core->stat.uop_decode2alloc_slip += uop->timing.when_allocated - uop->timing.when_decoded;)
        ZESTO_STAT(core->stat.uop_alloc2ready_slip += uop->timing.when_ready - uop->timing.when_allocated;)
        ZESTO_STAT(core->stat.uop_ready2issue_slip += uop->timing.when_issued - uop->timing.when_ready;)
        ZESTO_STAT(core->stat.uop_issue2exec_slip += uop->timing.when_exec - uop->timing.when_issued;)
        ZESTO_STAT(core->stat.uop_exec2complete_slip += uop->timing.when_completed - uop->timing.when_exec;)
        ZESTO_STAT(core->stat.uop_complete2commit_slip += core->sim_cycle - uop->timing.when_completed;)

        zesto_assert(uop->timing.when_exec != TICK_T_MAX,(void)0);
      }

      for (size_t oreg = 0; oreg < MAX_ODEPS; oreg++) {
        if(x86::is_ireg(uop->decode.odep_name[oreg]))
          core->stat.regfile_writes++;
        else if(x86::is_freg(uop->decode.odep_name[oreg]))
          core->stat.fp_regfile_writes++;
      }

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
          ZESTO_STAT(core->stat.commit_bytes += Mop->fetch.len;)
        }

        if(Mop->decode.is_ctrl)
          branches_committed++;

        ZESTO_STAT(core->stat.commit_uops += Mop->stat.num_uops;)
        ZESTO_STAT(core->stat.commit_eff_uops += Mop->stat.num_eff_uops;)
        ZESTO_STAT(core->stat.commit_branches += Mop->stat.num_branches;)
        ZESTO_STAT(core->stat.commit_refs += Mop->stat.num_refs;)
        ZESTO_STAT(core->stat.commit_loads += Mop->stat.num_loads;)
        if(Mop->decode.has_rep)
        {
          if(Mop->uop[Mop->decode.last_uop_index].decode.EOM)
            ZESTO_STAT(core->stat.commit_rep_insn++;)
          if(!Mop->oracle.zero_rep)
            ZESTO_STAT(core->stat.commit_rep_iterations++;)
          ZESTO_STAT(core->stat.commit_rep_uops += Mop->stat.num_uops;)
        }
        if(Mop->stat.num_uops > knobs->decode.max_uops[0])
        {
          ZESTO_STAT(core->stat.commit_UROM_insn++;)
          ZESTO_STAT(core->stat.commit_UROM_uops += Mop->stat.num_uops;)
          ZESTO_STAT(core->stat.commit_UROM_eff_uops += Mop->stat.num_eff_uops;)
        }

        core->stat.flow_count += Mop->stat.num_uops;
        core->stat.eff_flow_count += Mop->stat.num_eff_uops;

        if(Mop->decode.has_rep) /* all iterations of a REP count as one macro-op! */
        {
          if(Mop->uop[0].decode.BOM)
          {
            when_rep_fetch_started = Mop->timing.when_fetch_started;
            when_rep_fetched = Mop->timing.when_fetched;
            zesto_assert(Mop->timing.when_fetched != TICK_T_MAX,(void)0);
            zesto_assert(Mop->timing.when_fetch_started != TICK_T_MAX,(void)0);
            when_rep_decode_started = Mop->timing.when_decode_started;
            when_rep_commit_started = Mop->timing.when_commit_started;
          }
          if(Mop->uop[Mop->decode.last_uop_index].decode.EOM)
          {
            ZESTO_STAT(stat_add_sample(core->stat.flow_histo, core->stat.flow_count);)
            ZESTO_STAT(stat_add_sample(core->stat.eff_flow_histo, core->stat.eff_flow_count);)
            core->stat.flow_count = 0;
            core->stat.eff_flow_count = 0;

            if(!Mop->decode.is_trap)
            {
              ZESTO_STAT(core->stat.Mop_fetch_slip += when_rep_fetched - when_rep_fetch_started;)
              ZESTO_STAT(core->stat.Mop_fetch2decode_slip += when_rep_decode_started - when_rep_fetched;)
              ZESTO_STAT(core->stat.Mop_decode_slip += Mop->timing.when_decode_finished - when_rep_decode_started;) /* from decode of first Mop's first uop to decode of last Mop's last uop */
              ZESTO_STAT(core->stat.Mop_decode2commit_slip += when_rep_commit_started - Mop->timing.when_decode_finished;)
              ZESTO_STAT(core->stat.Mop_commit_slip += Mop->timing.when_commit_finished - when_rep_commit_started;)
            }
            else
              ZESTO_STAT(core->stat.commit_traps++;)
          }
        }
        else
        {
          ZESTO_STAT(stat_add_sample(core->stat.flow_histo, core->stat.flow_count);)
          ZESTO_STAT(stat_add_sample(core->stat.eff_flow_histo, core->stat.eff_flow_count);)
          core->stat.flow_count = 0;
          core->stat.eff_flow_count = 0;

          if(!Mop->decode.is_trap)
          {
            ZESTO_STAT(core->stat.Mop_fetch_slip += Mop->timing.when_fetched - Mop->timing.when_fetch_started;)
            ZESTO_STAT(core->stat.Mop_fetch2decode_slip += Mop->timing.when_decode_started - Mop->timing.when_fetched;)
            ZESTO_STAT(core->stat.Mop_decode_slip += Mop->timing.when_decode_finished - Mop->timing.when_decode_started;)
            ZESTO_STAT(core->stat.Mop_decode2commit_slip += Mop->timing.when_commit_started - Mop->timing.when_decode_finished;)
            ZESTO_STAT(core->stat.Mop_commit_slip += Mop->timing.when_commit_finished - Mop->timing.when_commit_started;)
            zesto_assert(Mop->timing.when_commit_finished != TICK_T_MAX,(void)0);
            zesto_assert(Mop->timing.when_fetched != TICK_T_MAX,(void)0);
            zesto_assert(Mop->timing.when_fetch_started != TICK_T_MAX,(void)0);
            //zesto_assert(Mop->timing.when_fetched != 0,(void)0);
            //zesto_assert(Mop->timing.when_fetch_started != 0,(void)0);
          }
          else
            ZESTO_STAT(core->stat.commit_traps++;)
        }
#ifdef ZTRACE
        ztrace_print(Mop,"c|commit:EOM=%d:trap=%d|all uops in Mop committed; Mop retired",
          Mop->uop[Mop->decode.last_uop_index].decode.EOM,Mop->decode.is_trap);
#endif

        if (Mop->fetch.PC == system_knobs.stopwatch_start_pc) {
          ticker = core->sim_cycle;
        }
        if (Mop->fetch.PC == system_knobs.stopwatch_stop_pc) {
          ticker = core->sim_cycle - ticker;
          fprintf(stderr, "Measurement: %" PRId64 " cycles\n", ticker);
        }

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

void core_commit_DPM_t::IO_step()
{
  /* Compatibility: Simulation can call this */
}

/* Walk ROB from youngest uop until we find the requested Mop.
   (NOTE: We stop at any uop belonging to the Mop.  We assume
   that recovery only occurs on Mop boundaries.)
   Release resources (PREGs, RS/ROB/LSQ entries, etc. as we go).
   If Mop == NULL, we're blowing away the entire pipeline. */
void
core_commit_DPM_t::recover(const struct Mop_t * const Mop)
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

      if(dead_uop->decode.in_fusion)
      {
        zesto_assert(dead_uop->decode.is_fusion_head,(void)0);
        while(dead_uop->decode.fusion_next)
          dead_uop = dead_uop->decode.fusion_next;
        zesto_assert(dead_uop != dead_uop->decode.fusion_head,(void)0);
      }

      while(dead_uop)
      {
        /* squash this instruction - this invalidates all in-flight actions (e.g., uop execution, cache accesses) */
        dead_uop->exec.action_id = core->new_action_id();
        if(dead_uop->timing.when_allocated != TICK_T_MAX)
          ROB_eff_num--;

        /* update allocation scoreboard if appropriate */
        /* if(uop->alloc.RS_index != -1) *//* fusion messes up this check */
        if(dead_uop->timing.when_exec == TICK_T_MAX && (dead_uop->alloc.port_assignment != -1))
          core->alloc->RS_deallocate(dead_uop);

        if(dead_uop->alloc.RS_index != -1) /* currently in RS */
          core->exec->RS_deallocate(dead_uop);


        /* In the following, we have to check it the uop has even been allocated yet... this has
           to do with our non-atomic implementation of allocation for fused-uops */
        if(dead_uop->decode.is_load || dead_uop->decode.is_fence)
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

        if((!dead_uop->decode.in_fusion) || dead_uop->decode.is_fusion_head)
          dead_uop = NULL;
        else
        {
          /* this is ugly... need to traverse fused uop linked-list
             in reverse; most fused uops are short, so we're just
             going to re-traverse... if longer fused uops are
             supported, it may make sense to implement a
             doubly-linked list instead. */
          struct uop_t * p = ROB[index];
          while(p->decode.fusion_next != dead_uop)
            p = p->decode.fusion_next;

          dead_uop = p;
        }
      }

      ROB[index] = NULL;
      ROB_tail = moddec(ROB_tail,knobs->commit.ROB_size); //(ROB_tail-1+knobs->commit.ROB_size) % knobs->commit.ROB_size;
      ROB_num --;
      zesto_assert(ROB_num >= 0,(void)0);
      zesto_assert(ROB_eff_num >= 0,(void)0);

      index = moddec(index,knobs->commit.ROB_size); //(index-1+knobs->commit.ROB_size) % knobs->commit.ROB_size;
    }
  }
}

void
core_commit_DPM_t::recover(void)
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

      if(dead_uop->decode.in_fusion)
      {
        zesto_assert(dead_uop->decode.is_fusion_head,(void)0);
        while(dead_uop->decode.fusion_next)
          dead_uop = dead_uop->decode.fusion_next;
        zesto_assert(dead_uop != dead_uop->decode.fusion_head,(void)0);
      }

      while(dead_uop)
      {
        /* squash this instruction - this invalidates all in-flight actions (e.g., uop execution, cache accesses) */
        dead_uop->exec.action_id = core->new_action_id();
        if(dead_uop->timing.when_allocated != TICK_T_MAX)
          ROB_eff_num--;

        /* update allocation scoreboard if appropriate */
        /* if(uop->alloc.RS_index != -1) */
        /* fusion messes up this check */
        if(dead_uop->timing.when_exec == TICK_T_MAX && (dead_uop->alloc.port_assignment != -1))
          core->alloc->RS_deallocate(dead_uop);

        if(dead_uop->alloc.RS_index != -1) /* currently in RS */
          core->exec->RS_deallocate(dead_uop);


        /* In the following, we have to check it the uop has even
           been allocated yet... this has to do with our non-atomic
           implementation of allocation for fused-uops */
        if(dead_uop->decode.is_load || dead_uop->decode.is_fence)
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

        if((!dead_uop->decode.in_fusion) || dead_uop->decode.is_fusion_head)
          dead_uop = NULL;
        else
        {
          /* this is ugly... need to traverse fused uop linked-list
             in reverse; most fused uops are short, so we're just
             going to retraverse... if longer fused uops are
             supported, it may make sense to implement a
             doubly-linked list instead. */
          struct uop_t * p = ROB[index];
          while(p->decode.fusion_next != dead_uop)
            p = p->decode.fusion_next;

          dead_uop = p;
        }
      }

      ROB[index] = NULL;
      ROB_tail = moddec(ROB_tail,knobs->commit.ROB_size); //(ROB_tail-1+knobs->commit.ROB_size) % knobs->commit.ROB_size;
      ROB_num --;
      zesto_assert(ROB_num >= 0,(void)0);
      zesto_assert(ROB_eff_num >= 0,(void)0);

      index = moddec(index,knobs->commit.ROB_size); //(index-1+knobs->commit.ROB_size) % knobs->commit.ROB_size;
    }
  }

  core->exec->STQ_squash_senior();

  core->exec->recover_check_assertions();
  zesto_assert(ROB_num == 0,(void)0);
}

bool core_commit_DPM_t::ROB_available(void)
{
  struct core_knobs_t * knobs = core->knobs;
  return ROB_num < knobs->commit.ROB_size;
}

bool core_commit_DPM_t::ROB_empty(void)
{
  return 0 == ROB_num;
}

bool core_commit_DPM_t::pipe_empty(void)
{
  return 0 == ROB_num;
}

void core_commit_DPM_t::ROB_insert(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  ROB[ROB_tail] = uop;
  uop->alloc.ROB_index = ROB_tail;
  ROB_num++;
  ROB_eff_num++;
  ROB_tail = modinc(ROB_tail,knobs->commit.ROB_size); //(ROB_tail+1) % knobs->commit.ROB_size;
}

void core_commit_DPM_t::ROB_fuse_insert(struct uop_t * const uop)
{
  uop->alloc.ROB_index = uop->decode.fusion_head->alloc.ROB_index;
  ROB_eff_num++;
}

/* Dummy fucntions for compatibility with IO pipe */
void core_commit_DPM_t::pre_commit_insert(struct uop_t * const uop)
{
  fatal("shouldn't be called");
}
void core_commit_DPM_t::pre_commit_fused_insert(struct uop_t * const uop)
{
  fatal("shouldn't be called");
}
bool core_commit_DPM_t::pre_commit_available()
{
  fatal("shouldn't be called");
}
void core_commit_DPM_t::pre_commit_step()
{
  /* Compatibility: simulation can call this */
}
void core_commit_DPM_t::pre_commit_recover(struct Mop_t * const Mop)
{
  fatal("shouldn't be called");
}
int core_commit_DPM_t::squash_uop(struct uop_t * const uop)
{
  fatal("shouldn't be called");
}


#endif
