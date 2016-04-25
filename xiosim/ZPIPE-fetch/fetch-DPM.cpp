/* fetch-DPM.cpp - Detailed Pipeline Model */
/*
 * __COPYRIGHT__ GT
 */

#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(fetch_opt_string,"DPM")
		  || !strcasecmp(fetch_opt_string,"IO-DPM"))
    return std::make_unique<class core_fetch_DPM_t>(core);
#else
class core_fetch_DPM_t:public core_fetch_t
{
  enum fetch_stall_t {FSTALL_byteQ_FULL, /* byteQ is full */
                      FSTALL_TBR,      /* predicted taken */
                      FSTALL_EOL,      /* hit end of cache line */
                      FSTALL_SPLIT,    /* instruction split across two lines (special case of EOL) */
                      FSTALL_SYSCALL,  /* syscall waiting for pipe to clear */
                      FSTALL_ZPAGE,    /* fetch request from zeroth page of memory */
                      FSTALL_REP,      /* no need to fetch REP instruction */
                      FSTALL_ORACLE,   /* oracle stall on MopQ capacity */
                      FSTALL_num
                     };

  public:

  /* constructor, stats registration */
  core_fetch_DPM_t(struct core_t * const core);
  ~core_fetch_DPM_t();
  virtual void reg_stats(xiosim::stats::StatsDatabase* sdb);
  virtual void update_occupancy(void);

  /* simulate one cycle */
  virtual void pre_fetch(void);
  virtual bool do_fetch(void);
  virtual void post_fetch(void);

  /* decode interface */
  virtual bool Mop_available(void);
  virtual struct Mop_t * Mop_peek(void);
  virtual void Mop_consume(void);

  /* enqueue a jeclear event into the jeclear_pipe */
  virtual void jeclear_enqueue(struct Mop_t * const Mop, const md_addr_t New_PC);
  /* recover the front-end after the recover request actually
     reaches the front end. */
  virtual void recover(const md_addr_t new_PC); 

  protected:

  int byteQ_num;
  int IQ_num;
  int IQ_uop_num;
  int IQ_eff_uop_num;

  /* The byteQ contains the raw bytes (well, the addresses thereof).
     This is similar to the old IFQ in that the contents of the I$
     are delivered here.  Each entry contains raw bytes, and so
     this may correspond to many, or even less than one, instruction
     per entry. */
  struct byteQ_entry_t {
    md_addr_t addr;
    tick_t when_fetch_requested;
    tick_t when_fetched;
    tick_t when_translation_requested;
    tick_t when_translated;
    int num_Mop;
    int MopQ_first_index;
    seq_t action_id;
    struct core_t * core;
  } * byteQ;
  int byteQ_linemask; /* for masking out line offset */
  int byteQ_head;
  int byteQ_tail;

  /* buffer/queue between predecode and the main decode pipeline */
  struct Mop_t ** IQ;
  int IQ_head;
  int IQ_tail;

  /* predecode pipe: finds instruction boundaries and separates
     them out so that each entry corresponds to a single inst. */
  struct Mop_t *** pipe;

  /* branch misprediction recovery pipeline (or other pipe flushes)
     to model non-zero delays between the back end misprediction
     detection and the actual front-end recovery. */
  struct jeclear_pipe_t {
    md_addr_t New_PC;
    struct Mop_t * Mop;
    seq_t action_id;
  } * jeclear_pipe;

  static const char *fetch_stall_str[FSTALL_num];

  bool predecode_enqueue(struct Mop_t * const Mop);
  void byteQ_request(const md_addr_t lineaddr);
  bool byteQ_is_full(void);
  bool byteQ_already_requested(const md_addr_t addr);

  static void IL1_callback(void * const op);
  static void ITLB_callback(void * const op);
  static bool translated_callback(void * const op, const seq_t action_id);
  static seq_t get_byteQ_action_id(void * const op);

  enum fetch_stall_t stall_reason;
};

const char *core_fetch_DPM_t::fetch_stall_str[FSTALL_num] = {
  "byteQ full             ",
  "taken branch           ",
  "end of cache line      ",
  "inst split on two lines",
  "trap waiting on drain  ",
  "request for page zero  ",
  "no need to fetch REP   ",
  "oracle stall on MopQ   "
};


core_fetch_DPM_t::core_fetch_DPM_t(struct core_t * const arg_core):
  byteQ_num(0),
  IQ_num(0),
  IQ_uop_num(0),
  IQ_eff_uop_num(0),
  stall_reason(FSTALL_SYSCALL)
{
  struct core_knobs_t * knobs = arg_core->knobs;
  core = arg_core;

  /* must come after exec_init()! */

  /* bpred */
  bpred = std::make_unique<bpred_t>(
      core,
      knobs->fetch.num_bpred_components,
      knobs->fetch.bpred_opt_str,
      knobs->fetch.fusion_opt_str,
      knobs->fetch.dirjmpbtb_opt_str,
      knobs->fetch.indirjmpbtb_opt_str,
      knobs->fetch.ras_opt_str
    );

  if(knobs->fetch.jeclear_delay)
  {
    jeclear_pipe = (jeclear_pipe_t*) calloc(knobs->fetch.jeclear_delay,sizeof(*jeclear_pipe));
    if(!jeclear_pipe)
      fatal("couldn't calloc jeclear pipe");
  }

  create_caches();

  byteQ = (byteQ_entry_t*) calloc(knobs->fetch.byteQ_size,sizeof(*byteQ));
  byteQ_head = byteQ_tail = 0;
  if(!byteQ)
    fatal("couldn't calloc byteQ");

  byteQ_linemask = ~(md_addr_t)(knobs->fetch.byteQ_linesize - 1);

  pipe = (struct Mop_t***) calloc(knobs->fetch.depth,sizeof(*pipe));
  if(!pipe)
    fatal("couldn't calloc predecode pipe");

  for(int i=0;i<knobs->fetch.depth;i++)
  {
    pipe[i] = (struct Mop_t**) calloc(knobs->fetch.width,sizeof(**pipe));
    if(!pipe[i])
      fatal("couldn't calloc predecode pipe[%d]",i);
  }

  IQ = (struct Mop_t**) calloc(knobs->fetch.IQ_size,sizeof(*IQ));
  if(!IQ)
    fatal("couldn't calloc decode instruction queue");
  IQ_head = IQ_tail = 0;
}

core_fetch_DPM_t::~core_fetch_DPM_t() {
    free(IQ);

    for (int i = 0; i < core->knobs->fetch.depth; i++) {
        free(pipe[i]);
    }
    free(pipe);

    free(byteQ);

    free(jeclear_pipe);
}

void core_fetch_DPM_t::reg_stats(xiosim::stats::StatsDatabase* sdb) {
    int coreID = core->id;

    stat_reg_note(sdb, "\n#### BPRED STATS ####");
    bpred->reg_stats(sdb, core);

    stat_reg_note(sdb, "\n#### INST CACHE STATS ####");
    cache_reg_stats(sdb, core, core->memory.IL1.get());
    cache_reg_stats(sdb, core, core->memory.ITLB.get());

    stat_reg_note(sdb, "\n#### FETCH STATS ####");
    auto sim_cycle_st = stat_find_core_stat<tick_t>(sdb, coreID, "sim_cycle");
    assert(sim_cycle_st);

    auto& fetch_bytes_st = stat_reg_core_counter(sdb, true, coreID, "fetch_bytes",
                                                 "total number of bytes fetched",
                                                 &core->stat.fetch_bytes, 0, true, NULL);
    auto& fetch_insn_st = stat_reg_core_counter(sdb, true, coreID, "fetch_insn",
                                                "total number of instructions fetched",
                                                &core->stat.fetch_insn, 0, true, NULL);
    auto& fetch_uops_st =
            stat_reg_core_counter(sdb, true, coreID, "fetch_uops", "total number of uops fetched",
                                  &core->stat.fetch_uops, 0, true, NULL);
    auto& fetch_eff_uops_st = stat_reg_core_counter(sdb, true, coreID, "fetch_eff_uops",
                                                    "total number of effective uops fetched",
                                                    &core->stat.fetch_eff_uops, 0, true, NULL);

    stat_reg_core_formula(sdb, true, coreID, "fetch_BPC", "BPC (bytes per cycle) at fetch",
                          fetch_bytes_st / *sim_cycle_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "fetch_IPC", "IPC at fetch",
                          fetch_insn_st / *sim_cycle_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "fetch_uPC", "uPC at fetch",
                          fetch_uops_st / *sim_cycle_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "fetch_euPC", "euPC at fetch",
                          fetch_eff_uops_st / *sim_cycle_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "fetch_byte_per_inst",
                          "average bytes per instruction", fetch_bytes_st / fetch_insn_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "fetch_byte_per_uop", "average bytes per uop",
                          fetch_bytes_st / fetch_uops_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "fetch_byte_per_eff_uop",
                          "average bytes per effective uop", fetch_bytes_st / fetch_eff_uops_st,
                          NULL);

    core->stat.fetch_stall = stat_reg_core_dist(
            sdb, coreID, "fetch_stall", "breakdown of stalls in fetch", 0, FSTALL_num,
            (PF_COUNT | PF_PDF), NULL, fetch_stall_str, true, NULL);

    reg_core_queue_occupancy_stats(sdb, coreID, "byteQ",
                                   &core->stat.byteQ_occupancy, NULL, NULL);
    auto& predecode_bytes_st = stat_reg_core_counter(sdb, true, coreID, "predecode_bytes",
                                                     "total number of bytes predecoded",
                                                     &core->stat.predecode_bytes, 0, true, NULL);
    auto& predecode_insn_st = stat_reg_core_counter(sdb, true, coreID, "predecode_insn",
                                                    "total number of instructions predecoded",
                                                    &core->stat.predecode_insn, 0, true, NULL);
    auto& predecode_uops_st = stat_reg_core_counter(sdb, true, coreID, "predecode_uops",
                                                    "total number of uops predecoded",
                                                    &core->stat.predecode_uops, 0, true, NULL);
    auto& predecode_eff_uops_st = stat_reg_core_counter(
            sdb, true, coreID, "predecode_eff_uops", "total number of effective uops predecoded",
            &core->stat.predecode_eff_uops, 0, true, NULL);

    stat_reg_core_formula(sdb, true, coreID, "predecode_BPC",
                          "BPC (bytes per cycle) at predecode", predecode_bytes_st / *sim_cycle_st,
                          NULL);
    stat_reg_core_formula(sdb, true, coreID, "predecode_IPC", "IPC at predecode",
                          predecode_insn_st / *sim_cycle_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "predecode_uPC", "uPC at predecode",
                          predecode_uops_st / *sim_cycle_st, NULL);
    stat_reg_core_formula(sdb, true, coreID, "predecode_euPC", "euPC at predecode",
                          predecode_eff_uops_st / *sim_cycle_st, NULL);

    reg_core_queue_occupancy_stats(sdb, coreID, "IQ", &core->stat.IQ_occupancy,
                                   &core->stat.IQ_empty_cycles, &core->stat.IQ_full_cycles);
}

void core_fetch_DPM_t::update_occupancy(void)
{
    /* byteQ */
  core->stat.byteQ_occupancy += byteQ_num;

    /* IQ */
  core->stat.IQ_occupancy += IQ_num;
  if(IQ_num >= core->knobs->fetch.IQ_size)
    core->stat.IQ_full_cycles++;
  if(IQ_num <= 0)
    core->stat.IQ_empty_cycles++;
}


/******************************/
/* byteQ/I$ RELATED FUNCTIONS */
/******************************/

bool core_fetch_DPM_t::byteQ_is_full(void)
{
  struct core_knobs_t * knobs = core->knobs;
  return (byteQ_num >= knobs->fetch.byteQ_size);
}

/* is this line already the most recently requested? */
bool core_fetch_DPM_t::byteQ_already_requested(const md_addr_t addr)
{
  struct core_knobs_t * knobs = core->knobs;
  int index = moddec(byteQ_tail,knobs->fetch.byteQ_size); //(byteQ_tail-1+knobs->fetch.byteQ_size) % knobs->fetch.byteQ_size;
  if(byteQ_num && (byteQ[index].addr == (addr & byteQ_linemask)))
    return true;
  else
    return false;
}

/* Initiate a fetch request */
void core_fetch_DPM_t::byteQ_request(const md_addr_t lineaddr)
{
  struct core_knobs_t * knobs = core->knobs;
  /* this function assumes you already called byteQ_already_requested so that
     you don't double-request the same line */
  byteQ[byteQ_tail].when_fetch_requested = TICK_T_MAX;
  byteQ[byteQ_tail].when_fetched = TICK_T_MAX;
  byteQ[byteQ_tail].when_translation_requested = TICK_T_MAX;
  byteQ[byteQ_tail].when_translated = TICK_T_MAX;
  byteQ[byteQ_tail].addr = lineaddr;
  byteQ[byteQ_tail].action_id = core->new_action_id();
  byteQ[byteQ_tail].core = core;
  byteQ_tail = modinc(byteQ_tail,knobs->fetch.byteQ_size); //(byteQ_tail+1)%knobs->fetch.byteQ_size;
  byteQ_num++;

  return;
}

/* Callback functions used by the cache code to indicate when the IL1/ITLB lookups
   have been completed. */
void core_fetch_DPM_t::IL1_callback(void * const op)
{
  byteQ_entry_t * byteQ = (byteQ_entry_t*) op;
  byteQ->when_fetched = byteQ->core->sim_cycle;
}

void core_fetch_DPM_t::ITLB_callback(void * const op)
{
  byteQ_entry_t * byteQ = (byteQ_entry_t*) op;
  byteQ->when_translated = byteQ->core->sim_cycle;
}

bool core_fetch_DPM_t::translated_callback(void * const op, const seq_t action_id)
{
  byteQ_entry_t * byteQ = (byteQ_entry_t*) op;
  if(byteQ->action_id == action_id)
    return byteQ->when_translated <= byteQ->core->sim_cycle;
  else
    return true;
}

seq_t core_fetch_DPM_t::get_byteQ_action_id(void * const op)
{
  byteQ_entry_t * byteQ = (byteQ_entry_t*) op;
  return byteQ->action_id;
}

/* returns true if Mop successfully enqueued into predecode pipe */
bool core_fetch_DPM_t::predecode_enqueue(struct Mop_t * const Mop)
{
  struct core_knobs_t * knobs = core->knobs;
  int i;
  int free_index = knobs->fetch.width;

  /* Find the first free entry after the last occupied entry */
  for(i=knobs->fetch.width-1;i>=0;i--)
  {
    if(pipe[0][i])
    {
      free_index = i + 1;
      break;
    }
  }
  if(i<0) /* stage was completely empty */
    free_index = 0;

  if(free_index < knobs->fetch.width)
  {
    pipe[0][free_index] = Mop;
#ifdef ZTRACE
    ztrace_print(Mop,"f|PD|predecode pipe enqueue");
#endif
    return true;
  }
  else
    return false;
}

/************************/
/* MAIN FETCH FUNCTIONS */
/************************/

void core_fetch_DPM_t::post_fetch(void)
{
  struct core_knobs_t * knobs = core->knobs;
  int i;

  /* This gets processed here, so that demand misses from the DL1 get higher
     priority for accessing the L2 */
  lk_lock(&cache_lock, core->id+1);
  cache_process(core->memory.ITLB.get());
  cache_process(core->memory.IL1.get());
  lk_unlock(&cache_lock);

  /* check predecode pipe's last stage for instructions to put into the IQ */
  for(i=0;(i<knobs->fetch.width) && (IQ_num < knobs->fetch.IQ_size);i++)
  {
    if(pipe[knobs->fetch.depth-1][i])
    {
      struct Mop_t * Mop= pipe[knobs->fetch.depth-1][i];
      if(Mop->uop[Mop->decode.last_uop_index].decode.EOM)
      {
        ZESTO_STAT(core->stat.predecode_insn++;)
        ZESTO_STAT(core->stat.predecode_uops += Mop->stat.num_uops;)
        ZESTO_STAT(core->stat.predecode_eff_uops += Mop->stat.num_eff_uops;)
      }
      ZESTO_STAT(core->stat.predecode_bytes += Mop->fetch.len;)

      IQ[IQ_tail] = pipe[knobs->fetch.depth-1][i];
      pipe[knobs->fetch.depth-1][i] = NULL;
      IQ_tail = modinc(IQ_tail,knobs->fetch.IQ_size); //(IQ_tail + 1) % knobs->fetch.IQ_size;
      IQ_num ++;
      IQ_uop_num += Mop->stat.num_uops;
      IQ_eff_uop_num += Mop->stat.num_eff_uops;
#ifdef ZTRACE
    ztrace_print(Mop,"f|IQ|IQ enqueue");
#endif
    }
  }

  /* shuffle predecode pipe (non-serpentine) */
  for(i=knobs->fetch.depth-1;i>0;i--)
  {
    int j;
    int this_stage_free = true;
    for(j=0;j<knobs->fetch.width;j++)
    {
      if(pipe[i][j])
      {
        this_stage_free = false;
        break;
      }
    }

    if(this_stage_free)
    {
      for(j=0;j<knobs->fetch.width;j++)
      {
        pipe[i][j] = pipe[i-1][j];
        pipe[i-1][j] = NULL;
      }
    }
  }

  /* check byteQ for fetched/translated instructions and move to predecode pipe */
  int byteQ_index = byteQ_head;
  while((byteQ_num && byteQ[byteQ_index].when_fetched != TICK_T_MAX) &&
        (byteQ_num && byteQ[byteQ_index].when_translated != TICK_T_MAX))
  {
    if(byteQ[byteQ_index].num_Mop)
    {
      int MopQ_index = byteQ[byteQ_index].MopQ_first_index;
      struct Mop_t * Mop = core->oracle->get_Mop(MopQ_index);

      /* Enqueue Mop into the predecode pipeline */
      if(predecode_enqueue(Mop))
      {
        Mop->timing.when_fetched = core->sim_cycle;
        if(Mop->uop[Mop->decode.last_uop_index].decode.EOM)
        {
          ZESTO_STAT(core->stat.fetch_insn++;)
          ZESTO_STAT(core->stat.fetch_bytes += Mop->fetch.len;)
        }

        ZESTO_STAT(core->stat.fetch_uops += Mop->stat.num_uops;)
        ZESTO_STAT(core->stat.fetch_eff_uops += Mop->stat.num_eff_uops;)

        byteQ[byteQ_index].num_Mop--;
        byteQ[byteQ_index].MopQ_first_index = core->oracle->next_index(MopQ_index);
      }
      else
        break; /* decode pipe is full */
    }

    if(byteQ[byteQ_index].num_Mop <= 0)
    {
      /* consumed all insts from this fetch line */
      byteQ[byteQ_index].MopQ_first_index = -1;
      byteQ[byteQ_index].when_fetch_requested = TICK_T_MAX;
      byteQ[byteQ_index].when_fetched = TICK_T_MAX;
      byteQ[byteQ_index].when_translation_requested = TICK_T_MAX;
      byteQ[byteQ_index].when_translated = TICK_T_MAX;
      byteQ_num--;
      byteQ_head = modinc(byteQ_index,knobs->fetch.byteQ_size); //(byteQ_index+1)%knobs->fetch.byteQ_size;
    }
  }

  stall_reason = FSTALL_EOL;
}

void core_fetch_DPM_t::pre_fetch(void)
{
  struct core_knobs_t * knobs = core->knobs;
  int asid = core->asid;
  int i;

  ZESTO_STAT(stat_add_sample(core->stat.fetch_stall, (int)stall_reason);)

  /* check byteQ and send requests to I$/ITLB */
  int index = byteQ_head;
  for(i=0;i<byteQ_num;i++)
  {
    if(byteQ[index].when_fetch_requested == TICK_T_MAX)
    {
      if(cache_enqueuable(core->memory.IL1.get(), asid, byteQ[index].addr))
      {
        cache_enqueue(core, core->memory.IL1.get(), NULL, CACHE_READ, asid, byteQ[index].addr, byteQ[index].addr, byteQ[index].action_id, 0, NO_MSHR, &byteQ[index], IL1_callback, NULL, translated_callback, get_byteQ_action_id);
        byteQ[index].when_fetch_requested = core->sim_cycle;
        break;
      }
    }
    index = modinc(index,knobs->fetch.byteQ_size);
  }

  index = byteQ_head;
  for(i=0;i<byteQ_num;i++)
  {
    if(byteQ[index].when_translation_requested == TICK_T_MAX)
    {
      if(cache_enqueuable(core->memory.ITLB.get(), asid, memory::page_table_address(asid, byteQ[index].addr)))
      {
        cache_enqueue(core, core->memory.ITLB.get(), NULL, CACHE_READ, 0, asid, memory::page_table_address(asid, byteQ[index].addr), byteQ[index].action_id, 0, NO_MSHR, &byteQ[index], ITLB_callback, NULL, NULL, get_byteQ_action_id);
        byteQ[index].when_translation_requested = core->sim_cycle;
        break;
      }
    }
    index = modinc(index,knobs->fetch.byteQ_size);
  }

  /* process jeclears */
  if(knobs->fetch.jeclear_delay)
  {
    struct Mop_t * Mop = jeclear_pipe[knobs->fetch.jeclear_delay-1].Mop;
    md_addr_t New_PC = jeclear_pipe[knobs->fetch.jeclear_delay-1].New_PC;

    /* there's a jeclear and it's still valid */
    if(Mop && (jeclear_pipe[knobs->fetch.jeclear_delay-1].action_id == Mop->fetch.jeclear_action_id))
    {
#ifdef ZTRACE
      ztrace_print(Mop,"f|jeclear_pipe|jeclear dequeued");
#endif
      if(Mop->fetch.bpred_update)
        bpred->recover(Mop->fetch.bpred_update, (New_PC != Mop->fetch.ftPC));
      core->oracle->recover(Mop);
      core->commit->recover(Mop);
      core->exec->recover(Mop);
      core->alloc->recover(Mop);
      core->decode->recover(Mop);
      /*core->fetch->*/recover(New_PC);
      Mop->commit.jeclear_in_flight = false;
    }

    /* advance jeclear pipe */
    for(i=knobs->fetch.jeclear_delay-1;i>0;i--)
      jeclear_pipe[i] = jeclear_pipe[i-1];
    jeclear_pipe[0].Mop = NULL;
  }

}

// Returns true if we can fetch more Mops on the same cycle
bool core_fetch_DPM_t::do_fetch(void)
{
  struct core_knobs_t * knobs = core->knobs;
  md_addr_t current_line = PC & byteQ_linemask;
  struct Mop_t * Mop = NULL;

  /* Waiting for pipe to clear from system call/trap. */
  if (core->oracle->is_draining()) {
    stall_reason = FSTALL_SYSCALL;
    return false;
  }

  Mop = core->oracle->exec(PC);

  if(Mop && (memory::page_round_down(PC) == 0)) {
    zesto_assert(core->oracle->spec_mode, false);
    stall_reason = FSTALL_ZPAGE;
    return false;
  }

  if(!Mop) {
    stall_reason = FSTALL_ORACLE;
    return false;
  }

  md_addr_t start_PC = Mop->fetch.PC;
  md_addr_t end_PC = Mop->fetch.PC + Mop->fetch.len - 1; /* addr of last byte */

  /* We explicitly check for both the address of the first byte and the last
     byte, since x86 instructions have no alignment restrictions and therefore
     may end up getting split across more than one cache line.  If so, this
     can generate more than one IL1/ITLB lookup. */
  if(byteQ_already_requested(start_PC))
  {
#ifdef ZTRACE
    if(!Mop->fetch.first_byte_requested)
      ztrace_print(Mop,"f|byteQ|first byte requested");
#endif
    Mop->fetch.first_byte_requested = true;
    if(Mop->timing.when_fetch_started == TICK_T_MAX)
      Mop->timing.when_fetch_started = core->sim_cycle;
  }
  if(!Mop->fetch.first_byte_requested)
  {
    if(byteQ_is_full()) /* stall if byteQ is full */
    {
      stall_reason = FSTALL_byteQ_FULL;
      return false;
    }

    if(Mop->timing.when_fetch_started == TICK_T_MAX)
      Mop->timing.when_fetch_started = core->sim_cycle;
#ifdef ZTRACE
    if(!Mop->fetch.first_byte_requested)
      ztrace_print(Mop,"f|byteQ|first byte requested");
#endif
    Mop->fetch.first_byte_requested = true;
    byteQ_request(start_PC & byteQ_linemask);
  }

  if(byteQ_already_requested(end_PC)) /* this should hit if end_PC is on same cache line as start_PC */
  {
#ifdef ZTRACE
    if(!Mop->fetch.last_byte_requested)
      ztrace_print(Mop,"f|byteQ|last byte requested");
#endif
    Mop->fetch.last_byte_requested = true;
  }
  if(!Mop->fetch.last_byte_requested)
  {
    /* if we reach here, then this implies that this inst crosses the I$ line boundary */

    if(byteQ_is_full()) /* stall if byteQ is full */
    {
      stall_reason = FSTALL_byteQ_FULL;
      return false;
    }

    /* this puts the request in right now, but it won't be acted upon until next
       cycle since the I$ can only handle one request per cycle and there's a check
       below for going off the end of a line that'll terminate the outer while loop. */
    Mop->fetch.last_byte_requested = true;
#ifdef ZTRACE
    ztrace_print(Mop,"f|byteQ|last byte requested");
#endif
    byteQ_request(end_PC & byteQ_linemask);
  }

  zesto_assert(Mop->fetch.first_byte_requested && Mop->fetch.last_byte_requested,false);

  /* All bytes for this Mop have been requested.  Record it in the byteQ entry
     and let the oracle know we're done with it so can proceed to the next
     one */

  int byteQ_index = moddec(byteQ_tail,knobs->fetch.byteQ_size); //(byteQ_tail-1+knobs->fetch.byteQ_size)%knobs->fetch.byteQ_size;
  if(byteQ[byteQ_index].num_Mop == 0)
    byteQ[byteQ_index].MopQ_first_index = core->oracle->get_index(Mop);
  byteQ[byteQ_index].num_Mop++;

  core->oracle->consume(Mop);

  /* figure out where to fetch from next */
  if(Mop->decode.is_ctrl || Mop->decode.has_rep)  /* XXX: illegal use of decode information */
  {
    Mop->fetch.bpred_update = bpred->get_state_cache();

    Mop->fetch.pred_NPC = bpred->lookup(Mop->fetch.bpred_update,
        Mop->decode.opflags, Mop->fetch.PC, Mop->fetch.ftPC, Mop->decode.targetPC,
        Mop->oracle.NextPC, Mop->oracle.taken_branch);

    bpred->spec_update(Mop->fetch.bpred_update, Mop->decode.opflags,
        Mop->fetch.PC, Mop->decode.targetPC, Mop->oracle.NextPC, Mop->fetch.bpred_update->our_pred);
#ifdef ZTRACE
    ztrace_print(Mop,"f|pred_targ=%" PRIxPTR"|target %s", Mop->fetch.pred_NPC, (Mop->fetch.pred_NPC == Mop->oracle.NextPC) ? "correct" : "mispred");
#endif
  }
  else
    Mop->fetch.pred_NPC = Mop->oracle.NextPC;

  if(Mop->fetch.pred_NPC != Mop->oracle.NextPC)
  {
    Mop->oracle.recover_inst = true;
    Mop->uop[Mop->decode.last_uop_index].oracle.recover_inst = true;
  }

  /* advance the fetch PC to the next instruction */
  PC = Mop->fetch.pred_NPC;

  ZTRACE_PRINT(core->id, "After bpred. PC: %" PRIxPTR", oracle.NPC: %" PRIxPTR", spec: %d, nuked_Mops: %d\n", PC, Mop->oracle.NextPC, core->oracle->spec_mode, core->oracle->num_Mops_before_feeder());

  if(Mop->oracle.taken_branch)
  {
    stall_reason = FSTALL_TBR;
    return false;
  }
  else if((end_PC & byteQ_linemask) != current_line)
  {
    stall_reason = FSTALL_SPLIT;
    return false;
  }
  else if((Mop->fetch.PC & byteQ_linemask) != current_line)
  {
    stall_reason = FSTALL_EOL;
    return false;
  }
  /* Stall on REPs similarly to taken branches, otherwise they easily flood the oracle. */
  /* XXX: We should generalize this for a loop-stream decoder */
  else if(Mop->decode.has_rep && PC == Mop->oracle.NextPC)
  {
    stall_reason = FSTALL_REP;
    return false;
  }
  else if(core->oracle->is_draining())
  {
    stall_reason = FSTALL_SYSCALL;
    return false;
  }

  /* still fetching from the same byteQ entry */
  return ((PC & byteQ_linemask) == current_line);
}

/*void core_fetch_DPM_t::step(void)
{
   post_fetch();
   while(do_fetch());
   pre_fetch();
}*/

bool core_fetch_DPM_t::Mop_available(void)
{
  return IQ_num > 0;
}

struct Mop_t * core_fetch_DPM_t::Mop_peek(void)
{
  return IQ[IQ_head];
}

void core_fetch_DPM_t::Mop_consume(void)
{
  struct core_knobs_t * knobs = core->knobs;
  struct Mop_t * Mop = IQ[IQ_head];
  IQ[IQ_head] = NULL;
  IQ_head = modinc(IQ_head,knobs->fetch.IQ_size); //(IQ_head + 1) % knobs->fetch.IQ_size;
  IQ_num --;
  IQ_uop_num -= Mop->stat.num_uops;
  IQ_eff_uop_num -= Mop->stat.num_eff_uops;
  zesto_assert(IQ_num >= 0,(void)0);
  zesto_assert(IQ_uop_num >= 0,(void)0);
  zesto_assert(IQ_eff_uop_num >= 0,(void)0);
}

void core_fetch_DPM_t::jeclear_enqueue(struct Mop_t * const Mop, const md_addr_t New_PC)
{
  if(jeclear_pipe[0].Mop) /* already a jeclear this cycle */
  {
    /* keep *older* of the two (don't need to process jeclear
       that's in the shadow of an earlier jeclear) */
    if(Mop->oracle.seq < jeclear_pipe[0].Mop->oracle.seq)
    {
      Mop->fetch.jeclear_action_id = core->new_action_id();
      Mop->commit.jeclear_in_flight = true;
      jeclear_pipe[0].Mop = Mop;
      jeclear_pipe[0].New_PC = New_PC;
      jeclear_pipe[0].action_id = Mop->fetch.jeclear_action_id;
#ifdef ZTRACE
      ztrace_print(Mop,"f|jeclear_pipe|enqueued");
#endif
    }
  }
  else
  {
    Mop->fetch.jeclear_action_id = core->new_action_id();
    Mop->commit.jeclear_in_flight = true;
    jeclear_pipe[0].Mop = Mop;
    jeclear_pipe[0].New_PC = New_PC;
    jeclear_pipe[0].action_id = Mop->fetch.jeclear_action_id;
#ifdef ZTRACE
    ztrace_print(Mop,"f|jeclear_pipe|enqueued");
#endif
  }
}

void
core_fetch_DPM_t::recover(const md_addr_t new_PC)
{
  struct core_knobs_t * knobs = core->knobs;
  /* XXX: use non-oracle nextPC as there may be multiple re-fetches */
  int i;
  PC = new_PC;

  /* clear out the byteQ */
  for(i=0;i<knobs->fetch.byteQ_size;i++)
  {
    byteQ[i].addr = 0;
    byteQ[i].when_fetch_requested = TICK_T_MAX;
    byteQ[i].when_fetched = TICK_T_MAX;
    byteQ[i].when_translation_requested = TICK_T_MAX;
    byteQ[i].when_translated = TICK_T_MAX;
    byteQ[i].num_Mop = 0;
    byteQ[i].MopQ_first_index = -1;
    byteQ[i].action_id = core->new_action_id();
    byteQ[i].core = core;
  }
  byteQ_num = 0;
  byteQ_head = 0;
  byteQ_tail = 0;

  memzero(IQ,knobs->fetch.IQ_size*sizeof(*IQ));
  IQ_num = 0;
  IQ_uop_num = 0;
  IQ_eff_uop_num = 0;
  IQ_head = 0;
  IQ_tail = 0;

  for(i=0;i<knobs->fetch.depth;i++)
  {
    int j;
    for(j=0;j<knobs->fetch.width;j++)
      pipe[i][j] = NULL;
  }
}

#endif
