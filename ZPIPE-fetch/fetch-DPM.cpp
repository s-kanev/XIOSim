/* fetch-DPM.cpp - Detailed Pipeline Model */
/*
 * __COPYRIGHT__ GT
 */

#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(fetch_opt_string,"DPM")
		  || !strcasecmp(fetch_opt_string,"IO-DPM"))
    return new core_fetch_DPM_t(core);
#else

class core_fetch_DPM_t:public core_fetch_t
{
  enum fetch_stall_t {FSTALL_byteQ_FULL, /* byteQ is full */
                      FSTALL_TBR,      /* predicted taken */
                      FSTALL_EOL,      /* hit end of cache line */
                      FSTALL_SPLIT,    /* instruction split across two lines (special case of EOL) */
                      FSTALL_BOGUS,    /* encountered invalid inst on wrong-path */
                      FSTALL_SYSCALL,  /* syscall waiting for pipe to clear */
                      FSTALL_ZPAGE,    /* fetch request from zeroth page of memory */
                      FSTALL_num
                     };

  public:

  /* constructor, stats registration */
  core_fetch_DPM_t(struct core_t * const core);
  virtual void reg_stats(struct stat_sdb_t * const sdb);
  virtual void update_occupancy(void);

  /* simulate one cycle */
  virtual void step(void);

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

};

const char *core_fetch_DPM_t::fetch_stall_str[FSTALL_num] = {
  "byteQ full             ",
  "taken branch           ",
  "end of cache line      ",
  "inst split on two lines",
  "wrong-path invalid inst",
  "trap waiting on drain  ",
  "request for page zero  "
};


core_fetch_DPM_t::core_fetch_DPM_t(struct core_t * const arg_core):
  byteQ_num(0),
  IQ_num(0),
  IQ_uop_num(0),
  IQ_eff_uop_num(0)
{
  struct core_knobs_t * knobs = arg_core->knobs;
  core = arg_core;

  /* must come after exec_init()! */
  char name[256];
  int sets, assoc, linesize, latency, banks, bank_width, MSHR_entries;
  char rp;
  int i;

  /* bpred */
  bpred = new bpred_t(
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

  /* IL1 */
  if(sscanf(knobs->memory.IL1_opt_str,"%[^:]:%d:%d:%d:%d:%d:%d:%c:%d",
      name,&sets,&assoc,&linesize,&banks,&bank_width,&latency, &rp, &MSHR_entries) != 9)
    fatal("invalid IL1 options: <name:sets:assoc:linesize:banks:bank_width:latency:repl-policy:num-MSHR>\n\t(%s)",knobs->memory.IL1_opt_str);

  /* the write-related options don't matter since the IL1 will(should) never see any stores */
  if(core->memory.DL2)
    core->memory.IL1 = cache_create(core,name,CACHE_READONLY,sets,assoc,linesize,rp,'w','t','n',banks,bank_width,latency,4,MSHR_entries,1,core->memory.DL2,core->memory.DL2_bus);
  else
    core->memory.IL1 = cache_create(core,name,CACHE_READONLY,sets,assoc,linesize,rp,'w','t','n',banks,bank_width,latency,4,MSHR_entries,1,uncore->LLC,uncore->LLC_bus);
  core->memory.IL1->MSHR_cmd_order = NULL;

  core->memory.IL1->PFF_size = knobs->memory.IL1_PFFsize;
  core->memory.IL1->PFF = (cache_t::PFF_t *) calloc(knobs->memory.IL1_PFFsize,sizeof(*core->memory.IL1->PFF));
  if(!core->memory.IL1->PFF)
    fatal("failed to calloc %s's prefetch FIFO",core->memory.IL1->name);
  prefetch_buffer_create(core->memory.IL1,knobs->memory.IL1_PF_buffer_size);
  prefetch_filter_create(core->memory.IL1,knobs->memory.IL1_PF_filter_size,knobs->memory.IL1_PF_filter_reset);
  core->memory.IL1->prefetch_threshold = knobs->memory.IL1_PFthresh;
  core->memory.IL1->prefetch_max = knobs->memory.IL1_PFmax;
  core->memory.IL1->PF_low_watermark = knobs->memory.IL1_low_watermark;
  core->memory.IL1->PF_high_watermark = knobs->memory.IL1_high_watermark;
  core->memory.IL1->PF_sample_interval = knobs->memory.IL1_WMinterval;

  core->memory.IL1->prefetcher = (struct prefetch_t **) calloc(knobs->memory.IL1_num_PF,sizeof(*core->memory.IL1->prefetcher));
  core->memory.IL1->num_prefetchers = knobs->memory.IL1_num_PF;
  if(!core->memory.IL1->prefetcher)
    fatal("couldn't calloc %s's prefetcher array",core->memory.IL1->name);
  for(i=0;i<knobs->memory.IL1_num_PF;i++)
    core->memory.IL1->prefetcher[i] = prefetch_create(knobs->memory.IL1PF_opt_str[i],core->memory.IL1);
  if(core->memory.IL1->prefetcher[0] == NULL)
    core->memory.IL1->num_prefetchers = knobs->memory.IL1_num_PF = 0;
  core->memory.IL1->prefetch_on_miss = knobs->memory.IL1_PF_on_miss;

  /* ITLB */
  if(sscanf(knobs->memory.ITLB_opt_str,"%[^:]:%d:%d:%d:%d:%c:%d",
      name,&sets,&assoc,&banks,&latency, &rp, &MSHR_entries) != 7)
    fatal("invalid ITLB options: <name:sets:assoc:banks:latency:repl-policy:num-MSHR>");

  if(core->memory.DL2)
    core->memory.ITLB = cache_create(core,name,CACHE_READONLY,sets,assoc,1,rp,'w','t','n',banks,1,latency,4,MSHR_entries,1,core->memory.DL2,core->memory.DL2_bus);
  else
    core->memory.ITLB = cache_create(core,name,CACHE_READONLY,sets,assoc,1,rp,'w','t','n',banks,1,latency,4,MSHR_entries,1,uncore->LLC,uncore->LLC_bus);
  core->memory.ITLB->MSHR_cmd_order = NULL;

  byteQ = (byteQ_entry_t*) calloc(knobs->fetch.byteQ_size,sizeof(*byteQ));
  byteQ_head = byteQ_tail = 0;
  if(!byteQ)
    fatal("couldn't calloc byteQ");

  byteQ_linemask = ~(md_addr_t)(knobs->fetch.byteQ_linesize - 1);

  pipe = (struct Mop_t***) calloc(knobs->fetch.depth,sizeof(*pipe));
  if(!pipe)
    fatal("couldn't calloc predecode pipe");

  for(i=0;i<knobs->fetch.depth;i++)
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

void
core_fetch_DPM_t::reg_stats(struct stat_sdb_t * const sdb)
{
  char buf[1024];
  char buf2[1024];
  struct thread_t * arch = core->current_thread;

  stat_reg_note(sdb,"\n#### BPRED STATS ####");
  bpred->reg_stats(sdb,core);

  stat_reg_note(sdb,"\n#### INST CACHE STATS ####");
  cache_reg_stats(sdb, core, core->memory.IL1);
  cache_reg_stats(sdb, core, core->memory.ITLB);

  stat_reg_note(sdb,"\n#### FETCH STATS ####");
  sprintf(buf,"c%d.fetch_bytes",arch->id);
  stat_reg_counter(sdb, true, buf, "total number of bytes fetched", &core->stat.fetch_bytes, core->stat.fetch_bytes, NULL);
  sprintf(buf,"c%d.fetch_insn",arch->id);
  stat_reg_counter(sdb, true, buf, "total number of instructions fetched", &core->stat.fetch_insn, core->stat.fetch_insn, NULL);
  sprintf(buf,"c%d.fetch_uops",arch->id);
  stat_reg_counter(sdb, true, buf, "total number of uops fetched", &core->stat.fetch_uops, core->stat.fetch_uops, NULL);
  sprintf(buf,"c%d.fetch_eff_uops",arch->id);
  stat_reg_counter(sdb, true, buf, "total number of effective uops fetched", &core->stat.fetch_eff_uops, core->stat.fetch_eff_uops, NULL);
  sprintf(buf,"c%d.fetch_BPC",arch->id);
  sprintf(buf2,"c%d.fetch_bytes/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "BPC (bytes per cycle) at fetch", buf2, NULL);
  sprintf(buf,"c%d.fetch_IPC",arch->id);
  sprintf(buf2,"c%d.fetch_insn/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "IPC at fetch", buf2, NULL);
  sprintf(buf,"c%d.fetch_uPC",arch->id);
  sprintf(buf2,"c%d.fetch_uops/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "uPC at fetch", buf2, NULL);
  sprintf(buf,"c%d.fetch_euPC",arch->id);
  sprintf(buf2,"c%d.fetch_eff_uops/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "euPC at fetch", buf2, NULL);
  sprintf(buf,"c%d.fetch_byte_per_inst",arch->id);
  sprintf(buf2,"c%d.fetch_bytes/c%d.fetch_insn",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "average bytes per instruction", buf2, NULL);
  sprintf(buf,"c%d.fetch_byte_per_uop",arch->id);
  sprintf(buf2,"c%d.fetch_bytes/c%d.fetch_uops",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "average bytes per uop", buf2, NULL);
  sprintf(buf,"c%d.fetch_byte_per_eff_uop",arch->id);
  sprintf(buf2,"c%d.fetch_bytes/c%d.fetch_eff_uops",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "average bytes per effective uop", buf2, NULL);

  sprintf(buf,"c%d.fetch_stall",core->current_thread->id);
  core->stat.fetch_stall = stat_reg_dist(sdb, buf,
                                          "breakdown of stalls in fetch",
                                          /* initial value */0,
                                          /* array size */FSTALL_num,
                                          /* bucket size */1,
                                          /* print format */(PF_COUNT|PF_PDF),
                                          /* format */NULL,
                                          /* index map */fetch_stall_str,
                                          /* print fn */NULL);

  sprintf(buf,"c%d.byteQ_occupancy",arch->id);
  stat_reg_counter(sdb, false, buf, "total byteQ occupancy (in lines/entries)", &core->stat.byteQ_occupancy, core->stat.byteQ_occupancy, NULL);
  sprintf(buf,"c%d.byteQ_avg",arch->id);
  sprintf(buf2,"c%d.byteQ_occupancy/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "average byteQ occupancy (in insts)", buf2, NULL);
  sprintf(buf,"c%d.predecode_bytes",arch->id);
  stat_reg_counter(sdb, true, buf, "total number of bytes predecoded", &core->stat.predecode_bytes, core->stat.predecode_bytes, NULL);
  sprintf(buf,"c%d.predecode_insn",arch->id);
  stat_reg_counter(sdb, true, buf, "total number of instructions predecoded", &core->stat.predecode_insn, core->stat.predecode_insn, NULL);
  sprintf(buf,"c%d.predecode_uops",arch->id);
  stat_reg_counter(sdb, true, buf, "total number of uops predecoded", &core->stat.predecode_uops, core->stat.predecode_uops, NULL);
  sprintf(buf,"c%d.predecode_eff_uops",arch->id);
  stat_reg_counter(sdb, true, buf, "total number of effective uops predecoded", &core->stat.predecode_eff_uops, core->stat.predecode_eff_uops, NULL);
  sprintf(buf,"c%d.predecode_BPC",arch->id);
  sprintf(buf2,"c%d.predecode_bytes/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "BPC (bytes per cycle) at predecode", buf2, NULL);
  sprintf(buf,"c%d.predecode_IPC",arch->id);
  sprintf(buf2,"c%d.predecode_insn/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "IPC at predecode", buf2, NULL);
  sprintf(buf,"c%d.predecode_uPC",arch->id);
  sprintf(buf2,"c%d.predecode_uops/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "uPC at predecode", buf2, NULL);
  sprintf(buf,"c%d.predecode_euPC",arch->id);
  sprintf(buf2,"c%d.predecode_eff_uops/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "euPC at predecode", buf2, NULL);

  sprintf(buf,"c%d.IQ_occupancy",arch->id);
  stat_reg_counter(sdb, false, buf, "total IQ occupancy (in insts)", &core->stat.IQ_occupancy, core->stat.IQ_occupancy, NULL);
  sprintf(buf,"c%d.IQ_uop_occupancy",arch->id);
  stat_reg_counter(sdb, false, buf, "total IQ occupancy (in uops)", &core->stat.IQ_uop_occupancy, core->stat.IQ_uop_occupancy, NULL);
  sprintf(buf,"c%d.IQ_eff_uop_occupancy",arch->id);
  stat_reg_counter(sdb, false, buf, "total IQ occupancy (in effective uops)", &core->stat.IQ_eff_uop_occupancy, core->stat.IQ_eff_uop_occupancy, NULL);
  sprintf(buf,"c%d.IQ_empty",arch->id);
  stat_reg_counter(sdb, false, buf, "total cycles IQ was empty", &core->stat.IQ_empty_cycles, core->stat.IQ_empty_cycles, NULL);
  sprintf(buf,"c%d.IQ_full",arch->id);
  stat_reg_counter(sdb, false, buf, "total cycles IQ was full", &core->stat.IQ_full_cycles, core->stat.IQ_full_cycles, NULL);
  sprintf(buf,"c%d.IQ_avg",arch->id);
  sprintf(buf2,"c%d.IQ_occupancy/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "average IQ occupancy (in insts)", buf2, NULL);
  sprintf(buf,"c%d.IQ_uop_avg",arch->id);
  sprintf(buf2,"c%d.IQ_uop_occupancy/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "average IQ occupancy (in uops)", buf2, NULL);
  sprintf(buf,"c%d.IQ_eff_uop_avg",arch->id);
  sprintf(buf2,"c%d.IQ_eff_uop_occupancy/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "average IQ occupancy (in effective uops)", buf2, NULL);
  sprintf(buf,"c%d.IQ_frac_empty",arch->id);
  sprintf(buf2,"c%d.IQ_empty/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "fraction of cycles IQ was empty", buf2, NULL);
  sprintf(buf,"c%d.IQ_frac_full",arch->id);
  sprintf(buf2,"c%d.IQ_full/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "fraction of cycles IQ was full", buf2, NULL);
}

void core_fetch_DPM_t::update_occupancy(void)
{
    /* byteQ */
  core->stat.byteQ_occupancy += byteQ_num;

    /* IQ */
  core->stat.IQ_occupancy += IQ_num;
  core->stat.IQ_uop_occupancy += IQ_uop_num;
  core->stat.IQ_eff_uop_occupancy += IQ_eff_uop_num;
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
  byteQ_tail = modinc(byteQ_tail,knobs->fetch.byteQ_size); //(byteQ_tail+1)%knobs->fetch.byteQ_size;
  byteQ_num++;

  return;
}

/* Callback functions used by the cache code to indicate when the IL1/ITLB lookups
   have been completed. */
void core_fetch_DPM_t::IL1_callback(void * const op)
{
  byteQ_entry_t * byteQ = (byteQ_entry_t*) op;
  byteQ->when_fetched = sim_cycle;
}

void core_fetch_DPM_t::ITLB_callback(void * const op)
{
  byteQ_entry_t * byteQ = (byteQ_entry_t*) op;
  byteQ->when_translated = sim_cycle;
}

bool core_fetch_DPM_t::translated_callback(void * const op, const seq_t action_id)
{
  byteQ_entry_t * byteQ = (byteQ_entry_t*) op;
  if(byteQ->action_id == action_id)
    return byteQ->when_translated <= sim_cycle;
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

void core_fetch_DPM_t::step(void)
{
  struct core_knobs_t * knobs = core->knobs;
  md_addr_t current_line = PC & byteQ_linemask;
  struct Mop_t * Mop = NULL;
  enum fetch_stall_t stall_reason = FSTALL_EOL;
  int i;

  /* This gets processed here, so that demand misses from the DL1 get higher
     priority for accessing the L2 */
  if(core->memory.ITLB->check_for_work) cache_process(core->memory.ITLB);
  if(core->memory.IL1->check_for_work) cache_process(core->memory.IL1);

  /* check predecode pipe's last stage for instructions to put into the IQ */
  for(i=0;(i<knobs->fetch.width) && (IQ_num < knobs->fetch.IQ_size);i++)
  {
    if(pipe[knobs->fetch.depth-1][i])
    {
      Mop = pipe[knobs->fetch.depth-1][i];
      if(Mop->uop[Mop->decode.last_uop_index].decode.EOM)
      {
        ZESTO_STAT(core->stat.predecode_insn++;)
        ZESTO_STAT(core->stat.predecode_uops += Mop->stat.num_uops;)
        ZESTO_STAT(core->stat.predecode_eff_uops += Mop->stat.num_eff_uops;)
      }
      ZESTO_STAT(core->stat.predecode_bytes += Mop->fetch.inst.len;)

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
        Mop->timing.when_fetched = sim_cycle;
        if(Mop->uop[Mop->decode.last_uop_index].decode.EOM)
        {
          ZESTO_STAT(core->stat.fetch_insn++;)
          ZESTO_STAT(core->stat.fetch_bytes += Mop->fetch.inst.len;) /* REP counts as only 1 fetch */
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


  while( /* still fetching from the same byteQ entry */
         (PC & byteQ_linemask) == current_line
       )
  {
    Mop = core->oracle->exec(PC);
    if(Mop && ((PC >> PAGE_SHIFT) == 0))
    {
      zesto_assert(core->oracle->spec_mode,(void)0);
      stall_reason = FSTALL_ZPAGE;
      break;
    }

    if(!Mop) /* awaiting pipe to clear for system call/trap, or encountered wrong-path bogus inst */
    {
      if(bogus)
        stall_reason = FSTALL_BOGUS;
      else
        stall_reason = FSTALL_SYSCALL;
      break;
    }

    md_addr_t start_PC = Mop->fetch.PC;
    md_addr_t end_PC = Mop->fetch.PC + Mop->fetch.inst.len - 1; /* addr of last byte */

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
        Mop->timing.when_fetch_started = sim_cycle;
    }
    if(!Mop->fetch.first_byte_requested)
    {
      if(byteQ_is_full()) /* stall if byteQ is full */
      {
        stall_reason = FSTALL_byteQ_FULL;
        break;
      }

      if(Mop->timing.when_fetch_started == TICK_T_MAX)
        Mop->timing.when_fetch_started = sim_cycle;
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
        break;
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

    zesto_assert(Mop->fetch.first_byte_requested && Mop->fetch.last_byte_requested,(void)0);

    /* All bytes for this Mop have been requested.  Record it in the byteQ entry
       and let the oracle know we're done with it so can proceed to the next
       one */

    int byteQ_index = moddec(byteQ_tail,knobs->fetch.byteQ_size); //(byteQ_tail-1+knobs->fetch.byteQ_size)%knobs->fetch.byteQ_size;
    if(byteQ[byteQ_index].num_Mop == 0)
      byteQ[byteQ_index].MopQ_first_index = core->oracle->get_index(Mop);
    byteQ[byteQ_index].num_Mop++;

    core->oracle->consume(Mop);

    /* figure out where to fetch from next */
    if(Mop->decode.is_ctrl || Mop->fetch.inst.rep)  /* XXX: illegal use of decode information */
    {
      Mop->fetch.bpred_update = bpred->get_state_cache();

      Mop->fetch.pred_NPC = bpred->lookup(Mop->fetch.bpred_update,
          Mop->decode.opflags, Mop->fetch.PC,Mop->fetch.PC+Mop->fetch.inst.len,Mop->decode.targetPC,
          Mop->oracle.NextPC,(Mop->oracle.NextPC != (Mop->fetch.PC+Mop->fetch.inst.len)));


      bpred->spec_update(Mop->fetch.bpred_update,Mop->decode.opflags,
          Mop->fetch.PC,Mop->decode.targetPC,Mop->oracle.NextPC,Mop->fetch.bpred_update->our_pred);
#ifdef ZTRACE
      bool pred_taken = (Mop->fetch.pred_NPC != (Mop->fetch.PC+Mop->fetch.inst.len));
      bool taken = (Mop->oracle.NextPC != (Mop->fetch.PC+Mop->fetch.inst.len));
      ztrace_print(Mop,"f|pred_dir=%x|direction %s",pred_taken,(pred_taken==taken)?"correct":"mispred");
      ztrace_print(Mop,"f|pred_targ=%x|target %s",Mop->fetch.pred_NPC,(Mop->fetch.pred_NPC==Mop->oracle.NextPC)?"correct":"mispred");
#endif
    }
    else
      Mop->fetch.pred_NPC = Mop->fetch.PC + Mop->fetch.inst.len;

    if(Mop->fetch.pred_NPC != Mop->oracle.NextPC)
    {
      Mop->oracle.recover_inst = true;
      Mop->uop[Mop->decode.last_uop_index].oracle.recover_inst = true;
    }

    /* advance the fetch PC to the next instruction */
    PC = Mop->fetch.pred_NPC;

    if(  (Mop->fetch.pred_NPC != (Mop->fetch.PC + Mop->fetch.inst.len))
      && (Mop->fetch.pred_NPC != Mop->fetch.PC)) /* REPs don't count as taken branches w.r.t. fetching */
    {
      stall_reason = FSTALL_TBR;
      break;
    }
    else if((end_PC & byteQ_linemask) != current_line)
    {
      stall_reason = FSTALL_SPLIT;
      break;
    }
    else if((Mop->fetch.PC & byteQ_linemask) != current_line)
    {
      stall_reason = FSTALL_EOL;
      break;
    }

  } /* while */

  ZESTO_STAT(stat_add_sample(core->stat.fetch_stall, (int)stall_reason);)

  /* check byteQ and send requests to I$/ITLB */
  int index = byteQ_head;
  for(i=0;i<byteQ_num;i++)
  {
    if(byteQ[index].when_fetch_requested == TICK_T_MAX)
    {
      if(cache_enqueuable(core->memory.IL1,core->current_thread->id,byteQ[index].addr))
      {
        cache_enqueue(core,core->memory.IL1,NULL,CACHE_READ,core->current_thread->id,byteQ[index].addr,byteQ[index].addr,byteQ[index].action_id,0,NO_MSHR,&byteQ[index],IL1_callback,NULL,translated_callback,get_byteQ_action_id);
        byteQ[index].when_fetch_requested = sim_cycle;
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
      if(cache_enqueuable(core->memory.ITLB,core->current_thread->id,PAGE_TABLE_ADDR(core->current_thread->id,byteQ[index].addr)))
      {
        cache_enqueue(core,core->memory.ITLB,NULL,CACHE_READ,0,core->current_thread->id,PAGE_TABLE_ADDR(core->current_thread->id,byteQ[index].addr),byteQ[index].action_id,0,NO_MSHR,&byteQ[index],ITLB_callback,NULL,NULL,get_byteQ_action_id);
        byteQ[index].when_translation_requested = sim_cycle;
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
        bpred->recover(Mop->fetch.bpred_update,(New_PC != (Mop->fetch.PC + Mop->fetch.inst.len)));
      core->oracle->recover(Mop);
      
      //XXX: A bit stupid - need the flush everything from the most speculative - reverse order
      core->exec->recover(Mop);
      core->commit->recover(Mop);
      
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
  bogus = false;

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
