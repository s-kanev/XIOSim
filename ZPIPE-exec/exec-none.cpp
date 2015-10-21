#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(exec_opt_string,"none"))
    return new core_exec_NONE_t(core);
#else

class core_exec_NONE_t:public core_exec_t
{
  public:

  core_exec_NONE_t(struct core_t * const core);
  virtual void reg_stats(struct stat_sdb_t * const sdb);
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


core_exec_NONE_t::core_exec_NONE_t(class core_t * const arg_core)
{
  struct core_knobs_t* knobs = arg_core->knobs;
  core = arg_core;

  char name[256];
  int sets, assoc, linesize, latency, banks, bank_width, MSHR_entries, MSHR_WB_entries;
  char rp, ap, wp, wc;

  /* note: caches must be instantiated from the level furthest from the core first (e.g., L2) */

  /* per-core DL2 */

  /* per-core L2 bus (between L1 and L2) */
  if(!strcasecmp(knobs->memory.DL2_opt_str,"none"))
  {
    core->memory.DL2 = NULL;
    core->memory.DL2_bus = NULL;
  }
  else
  {
    if(sscanf(knobs->memory.DL2_opt_str,"%[^:]:%d:%d:%d:%d:%d:%d:%c:%c:%c:%d:%d:%c",
        name,&sets,&assoc,&linesize,&banks,&bank_width,&latency,&rp,&ap,&wp, &MSHR_entries, &MSHR_WB_entries, &wc) != 13)
      fatal("invalid DL2 options: <name:sets:assoc:linesize:banks:bank-width:latency:repl-policy:alloc-policy:write-policy:num-MSHR:WB-buffers:write-combining>\n\t(%s)",knobs->memory.DL2_opt_str);
    core->memory.DL2 = cache_create(core,name,CACHE_READWRITE,sets,assoc,linesize,
                             rp,ap,wp,wc,banks,bank_width,latency,
                             MSHR_entries,MSHR_WB_entries,1,uncore->LLC,uncore->LLC_bus,
                             knobs->memory.DL2_magic_hit_rate);

    if(!knobs->memory.DL2_MSHR_cmd || !strcasecmp(knobs->memory.DL2_MSHR_cmd,"fcfs"))
      core->memory.DL2->MSHR_cmd_order = NULL;
    else
    {
      if(strlen(knobs->memory.DL2_MSHR_cmd) != 4)
        fatal("-dl1:mshr_cmd must either be \"fcfs\" or contain all four of [RWBP]");
      bool R_seen = false;
      bool W_seen = false;
      bool B_seen = false;
      bool P_seen = false;

      core->memory.DL2->MSHR_cmd_order = (enum cache_command*)calloc(4,sizeof(enum cache_command));
      if(!core->memory.DL2->MSHR_cmd_order)
        fatal("failed to calloc MSHR_cmd_order array for DL2");

      for(int c=0;c<4;c++)
      {
        switch(toupper(knobs->memory.DL2_MSHR_cmd[c]))
        {
          case 'R': core->memory.DL2->MSHR_cmd_order[c] = CACHE_READ; R_seen = true; break;
          case 'W': core->memory.DL2->MSHR_cmd_order[c] = CACHE_WRITE; W_seen = true; break;
          case 'B': core->memory.DL2->MSHR_cmd_order[c] = CACHE_WRITEBACK; B_seen = true; break;
          case 'P': core->memory.DL2->MSHR_cmd_order[c] = CACHE_PREFETCH; P_seen = true; break;
          default: fatal("unknown cache operation '%c' for -dl1:mshr_cmd; must be one of [RWBP]");
        }
      }
      if(!R_seen || !W_seen || !B_seen || !P_seen)
        fatal("-dl1:mshr_cmd must contain *each* of [RWBP]");
    }

    core->memory.DL2->prefetcher = (struct prefetch_t**) calloc(knobs->memory.DL2_num_PF,sizeof(*core->memory.DL2->prefetcher));
    core->memory.DL2->num_prefetchers = knobs->memory.DL2_num_PF;
    if(!core->memory.DL2->prefetcher)
      fatal("couldn't calloc %s's prefetcher array",core->memory.DL2->name);
    for(int i=0;i<knobs->memory.DL2_num_PF;i++)
      core->memory.DL2->prefetcher[i] = prefetch_create(knobs->memory.DL2PF_opt_str[i],core->memory.DL2);
    if(core->memory.DL2->prefetcher[0] == NULL)
      core->memory.DL2->num_prefetchers = knobs->memory.DL2_num_PF = 0;
    core->memory.DL2->prefetch_on_miss = knobs->memory.DL2_PF_on_miss;

    core->memory.DL2->PFF_size = knobs->memory.DL2_PFFsize;
    core->memory.DL2->PFF = (cache_t::PFF_t*) calloc(knobs->memory.DL2_PFFsize,sizeof(*core->memory.DL2->PFF));
    if(!core->memory.DL2->PFF)
      fatal("failed to calloc %s's prefetch FIFO",core->memory.DL2->name);
    core->memory.DL2->prefetch_threshold = knobs->memory.DL2_PFthresh;
    core->memory.DL2->prefetch_max = knobs->memory.DL2_PFmax;
    core->memory.DL2->PF_low_watermark = knobs->memory.DL2_low_watermark;
    core->memory.DL2->PF_high_watermark = knobs->memory.DL2_high_watermark;
    core->memory.DL2->PF_sample_interval = knobs->memory.DL2_WMinterval;

    core->memory.DL2_bus = bus_create("DL2_bus", core->memory.DL2->linesize*core->memory.DL2->banks, &core->sim_cycle, 1);
  }

  /* per-core DL1 */
  if(sscanf(knobs->memory.DL1_opt_str,"%[^:]:%d:%d:%d:%d:%d:%d:%c:%c:%c:%d:%d:%c",
      name,&sets,&assoc,&linesize,&banks,&bank_width,&latency,&rp,&ap,&wp, &MSHR_entries, &MSHR_WB_entries, &wc) != 13)
    fatal("invalid DL1 options: <name:sets:assoc:linesize:banks:bank-width:latency:repl-policy:alloc-policy:write-policy:num-MSHR:WB-buffers:write-combining>\n\t(%s)",knobs->memory.DL1_opt_str);

  if(core->memory.DL2)
    core->memory.DL1 = cache_create(core,name,CACHE_READWRITE,sets,assoc,linesize,
                             rp,ap,wp,wc,banks,bank_width,latency,
                             MSHR_entries,MSHR_WB_entries,1,core->memory.DL2,core->memory.DL2_bus,
                             knobs->memory.DL1_magic_hit_rate);
  else
    core->memory.DL1 = cache_create(core,name,CACHE_READWRITE,sets,assoc,linesize,
                             rp,ap,wp,wc,banks,bank_width,latency,
                             MSHR_entries,MSHR_WB_entries,1,uncore->LLC,uncore->LLC_bus,
                             knobs->memory.DL1_magic_hit_rate);
  if(!knobs->memory.DL1_MSHR_cmd || !strcasecmp(knobs->memory.DL1_MSHR_cmd,"fcfs"))
    core->memory.DL1->MSHR_cmd_order = NULL;
  else
  {
    if(strlen(knobs->memory.DL1_MSHR_cmd) != 4)
      fatal("-dl1:mshr_cmd must either be \"fcfs\" or contain all four of [RWBP]");
    bool R_seen = false;
    bool W_seen = false;
    bool B_seen = false;
    bool P_seen = false;

    core->memory.DL1->MSHR_cmd_order = (enum cache_command*)calloc(4,sizeof(enum cache_command));
    if(!core->memory.DL1->MSHR_cmd_order)
      fatal("failed to calloc MSHR_cmd_order array for DL1");

    for(int c=0;c<4;c++)
    {
      switch(toupper(knobs->memory.DL1_MSHR_cmd[c]))
      {
        case 'R': core->memory.DL1->MSHR_cmd_order[c] = CACHE_READ; R_seen = true; break;
        case 'W': core->memory.DL1->MSHR_cmd_order[c] = CACHE_WRITE; W_seen = true; break;
        case 'B': core->memory.DL1->MSHR_cmd_order[c] = CACHE_WRITEBACK; B_seen = true; break;
        case 'P': core->memory.DL1->MSHR_cmd_order[c] = CACHE_PREFETCH; P_seen = true; break;
        default: fatal("unknown cache operation '%c' for -dl1:mshr_cmd; must be one of [RWBP]");
      }
    }
    if(!R_seen || !W_seen || !B_seen || !P_seen)
      fatal("-dl1:mshr_cmd must contain *each* of [RWBP]");
  }

  core->memory.DL1->prefetcher = (struct prefetch_t**) calloc(knobs->memory.DL1_num_PF,sizeof(*core->memory.DL1->prefetcher));
  core->memory.DL1->num_prefetchers = knobs->memory.DL1_num_PF;
  if(!core->memory.DL1->prefetcher)
    fatal("couldn't calloc %s's prefetcher array",core->memory.DL1->name);
  for(int i=0;i<knobs->memory.DL1_num_PF;i++)
    core->memory.DL1->prefetcher[i] = prefetch_create(knobs->memory.DL1PF_opt_str[i],core->memory.DL1);
  if(core->memory.DL1->prefetcher[0] == NULL)
    core->memory.DL1->num_prefetchers = knobs->memory.DL1_num_PF = 0;
  core->memory.DL1->prefetch_on_miss = knobs->memory.DL1_PF_on_miss;

  core->memory.DL1->PFF_size = knobs->memory.DL1_PFFsize;
  core->memory.DL1->PFF = (cache_t::PFF_t*) calloc(knobs->memory.DL1_PFFsize,sizeof(*core->memory.DL1->PFF));
  if(!core->memory.DL1->PFF)
    fatal("failed to calloc %s's prefetch FIFO",core->memory.DL1->name);
  prefetch_buffer_create(core->memory.DL1,knobs->memory.DL1_PF_buffer_size);
  prefetch_filter_create(core->memory.DL1,knobs->memory.DL1_PF_filter_size,knobs->memory.DL1_PF_filter_reset);
  core->memory.DL1->prefetch_threshold = knobs->memory.DL1_PFthresh;
  core->memory.DL1->prefetch_max = knobs->memory.DL1_PFmax;
  core->memory.DL1->PF_low_watermark = knobs->memory.DL1_low_watermark;
  core->memory.DL1->PF_high_watermark = knobs->memory.DL1_high_watermark;
  core->memory.DL1->PF_sample_interval = knobs->memory.DL1_WMinterval;

  core->memory.DL1->controller = controller_create(knobs->memory.DL1_controller_opt_str, core, core->memory.DL1);
  if(core->memory.DL2 != NULL)
    core->memory.DL2->controller = controller_create(knobs->memory.DL2_controller_opt_str, core, core->memory.DL2);

  core->memory.mem_repeater = repeater_create(core->knobs->exec.repeater_opt_str, core, "MR1", core->memory.DL1);
}

void core_exec_NONE_t::reg_stats(struct stat_sdb_t * const sdb)
{
  stat_reg_note(sdb,"\n#### DATA CACHE STATS ####");
  cache_reg_stats(sdb, core, core->memory.DL1);
  if(core->memory.DL2)
    cache_reg_stats(sdb, core, core->memory.DL2);
}


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
  lk_lock(&cache_lock, core->id+1);
  if(core->memory.DL2 && core->memory.DL2->check_for_work) cache_process(core->memory.DL2);
  if(core->memory.DL1->check_for_work) cache_process(core->memory.DL1);
  lk_unlock(&cache_lock);

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

    if (cache_enqueuable(core->memory.DL1, asid, addr)) {
      // Mark uop as sent to caches
      uop->timing.when_exec = core->sim_cycle;

      if (cache_op == CACHE_READ) {
        cache_enqueue(core, core->memory.DL1, NULL, CACHE_READ, asid, pc, addr, 1, 0, NO_MSHR, uop, DL1_load_callback, NULL, translated_callback, get_uop_action_id);
      } else {
        // For writeback caches, we mark them as completed right away,
        // without waiting for them to go through the cache hierachy.
        cache_enqueue(core, core->memory.DL1, NULL, CACHE_WRITE, asid, pc, addr, 1, 0, NO_MSHR, NULL, DL1_store_callback, NULL, translated_callback, get_uop_action_id);
        uop->timing.when_completed = core->sim_cycle;
      }
    }
  }
}

#endif
