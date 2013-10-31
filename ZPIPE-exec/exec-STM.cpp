/* exec-STM.cpp - Simple(r) Timing Model */
/*
 * __COPYRIGHT__ GT
 */


#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(exec_opt_string,"STM"))
    return new core_exec_STM_t(core);
#else

class core_exec_STM_t:public core_exec_t
{
  /* readyQ for scheduling */
  struct readyQ_node_t {
    struct uop_t * uop;
    seq_t uop_seq; /* seq id of uop when inserted - for proper sorting even after uop recycled */
    seq_t action_id;
    tick_t when_assigned;
    struct readyQ_node_t * next;
  };

  /* struct for a squashable in-flight uop (for example, a uop making its way
     down an ALU pipeline).  Changing the original uop's tag will make the tags
     no longer match, thereby invalidating the in-flight action. */
  struct uop_action_t {
    struct uop_t * uop;
    seq_t action_id;
    tick_t pipe_exit_time;
  };

  /* struct for a generic pipelined ALU */
  struct ALU_t {
    struct uop_action_t * pipe; /* implemented as min-heap */
    int occupancy;
    int latency;    /* number of cycles from start of execution to end */
    int issue_rate; /* number of cycles between issuing independent instructions on this ALU */
    tick_t when_executable; /* cycle when next instruction can actually start executing on this ALU */
  };

  public:

  core_exec_STM_t(struct core_t * const core);
  virtual void reg_stats(struct stat_sdb_t * const sdb);
  virtual void freeze_stats(void);
  virtual void update_occupancy(void);
  virtual void reset_execution(void);

  virtual void ALU_exec(void);
  virtual void LDST_exec(void);
  virtual void RS_schedule(void);
  virtual void LDQ_schedule(void);

  virtual void recover(const struct Mop_t * const Mop);
  virtual void recover(void);

  virtual void insert_ready_uop(struct uop_t * const uop);

  virtual bool RS_available(void);
  virtual void RS_insert(struct uop_t * const uop);
  virtual void RS_fuse_insert(struct uop_t * const uop);
  virtual void RS_deallocate(struct uop_t * const uop);

  virtual bool LDQ_available(void);
  virtual void LDQ_insert(struct uop_t * const uop);
  virtual void LDQ_deallocate(struct uop_t * const uop);
  virtual void LDQ_squash(struct uop_t * const dead_uop);

  virtual bool STQ_available(void);
  virtual void STQ_insert_sta(struct uop_t * const uop);
  virtual void STQ_insert_std(struct uop_t * const uop);
  virtual void STQ_deallocate_sta(void);
  virtual bool STQ_deallocate_std(struct uop_t * const uop);
  virtual void STQ_deallocate_senior(void);
  virtual void STQ_squash_sta(struct uop_t * const dead_uop);
  virtual void STQ_squash_std(struct uop_t * const dead_uop);
  virtual void STQ_squash_senior(void);

  virtual void recover_check_assertions(void);

  virtual void step();
  virtual void exec_fuse_insert(struct uop_t * const uop);
  virtual bool exec_empty(void);
  virtual void exec_insert(struct uop_t * const uop);
  virtual bool port_available(int port_ind);
  virtual bool exec_fused_ST(struct uop_t * const uop);

  
  protected:
  struct readyQ_node_t * readyQ_free_pool; /* for scheduling readyQ's */
#ifdef DEBUG
  int readyQ_free_pool_debt; /* for debugging memory leaks */
#endif

  struct uop_t ** RS;
  int RS_num;

  struct LDQ_t {
    bool valid; /* is this entry allocated/ */
    struct uop_t * uop;
    md_addr_t virt_addr;
    md_paddr_t phys_addr;
    int mem_size;
    bool addr_valid;
    int store_color;   /* STQ index of most recent store before this load */
    bool hit_in_STQ;    /* received value from STQ */
    tick_t when_issued; /* when load actually issued */
  } * LDQ;
  int LDQ_head;
  int LDQ_tail;
  int LDQ_num;

  struct STQ_t {
    struct uop_t * sta;
    struct uop_t * std;
    seq_t uop_seq;
    md_addr_t virt_addr;
    md_paddr_t phys_addr;
    int mem_size;
    union val_t value;
    bool addr_valid;
    bool value_valid;
    int next_load; /* LDQ index of next load in program order */

    /* for commit */
    bool translation_complete; /* dtlb access finished */
    bool write_complete; /* write access finished */
    seq_t action_id; /* need to squash commits when a core resets (multi-core mode only) */
  } * STQ;
  int STQ_head;
  int STQ_tail;
  int STQ_num;

  struct exec_port_t {
    struct ALU_t * FU[NUM_FU_CLASSES];
    struct readyQ_node_t * readyQ;
    struct ALU_t * STQ; /* store-queue lookup/search pipeline for load execution */
    int num_FU_types; /* the number of FU's bound to this port */
    enum md_fu_class * FU_types; /* the corresponding types of those FUs */
  } * port;
  bool check_for_work; /* used to skip ALU exec when there's no uops */

  /* various exec utility functions */

  struct readyQ_node_t * get_readyQ_node(void);
  void return_readyQ_node(struct readyQ_node_t * const p);
  bool check_load_issue_conditions(const struct uop_t * const uop);

  void load_writeback(struct uop_t * const uop);

  /* callbacks need to be static */
  static void DL1_callback(void * const op);
  static void DTLB_callback(void * const op);
  static bool translated_callback(void * const op,const seq_t);
  static seq_t get_uop_action_id(void * const op);

  /* callbacks used by commit for stores */
  static void store_dl1_callback(void * const op);
  static void store_dtlb_callback(void * const op);
  static bool store_translated_callback(void * const op,const seq_t);

  /* min-heap priority-Q management functions */
  void ALU_heap_balance(struct uop_action_t * pipe, const int insert_position);
  void ALU_heap_remove(struct uop_action_t * pipe, const int pipe_num);

};

/*******************/
/* SETUP FUNCTIONS */
/*******************/

core_exec_STM_t::core_exec_STM_t(struct core_t * arg_core):
  readyQ_free_pool(NULL), 
#ifdef DEBUG
  readyQ_free_pool_debt(0),
#endif
  RS_num(0), LDQ_head(0), LDQ_tail(0), LDQ_num(0),
  STQ_head(0), STQ_tail(0), STQ_num(0)
{
  struct core_knobs_t * knobs = arg_core->knobs;
  core = arg_core;

  RS = (struct uop_t**) calloc(knobs->exec.RS_size,sizeof(*RS));
  if(!RS)
    fatal("couldn't calloc RS");

  LDQ = (core_exec_STM_t::LDQ_t*) calloc(knobs->exec.LDQ_size,sizeof(*LDQ));
  if(!LDQ)
    fatal("couldn't calloc LDQ");

  STQ = (core_exec_STM_t::STQ_t*) calloc(knobs->exec.STQ_size,sizeof(*STQ));
  if(!STQ)
    fatal("couldn't calloc STQ");

  int i;
  /* This shouldn't be necessary, but I threw it in because valgrind (memcheck) was reporting
     that STQ[i].sta was being used uninitialized. */
  for(i=0;i<knobs->exec.STQ_size;i++)
    STQ[i].sta = NULL;

  /*********************************************************/
  /* Data cache parsing first, functional units after this */
  /*********************************************************/
  char name[256];
  int sets, assoc, linesize, latency, banks, bank_width, MSHR_entries, MSHR_WB_entries;
  char rp, ap, wp, wc;
  
  /* note: caches must be instantiated from the level furthest from the core first (e.g., LLC) */

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
                             MSHR_entries,MSHR_WB_entries,1,uncore->LLC,uncore->LLC_bus);

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
    for(i=0;i<knobs->memory.DL2_num_PF;i++)
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

    core->memory.DL2_bus = bus_create("DL2_bus", core->memory.DL2->linesize, &core->sim_cycle, 1);
  }

  /* per-core DL1 */
  if(sscanf(knobs->memory.DL1_opt_str,"%[^:]:%d:%d:%d:%d:%d:%d:%c:%c:%c:%d:%d:%c",
      name,&sets,&assoc,&linesize,&banks,&bank_width,&latency,&rp,&ap,&wp, &MSHR_entries, &MSHR_WB_entries, &wc) != 13)
    fatal("invalid DL1 options: <name:sets:assoc:linesize:banks:bank-width:latency:repl-policy:alloc-policy:write-policy:num-MSHR:WB-buffers:write-combining>\n\t(%s)",knobs->memory.DL1_opt_str);

  if(core->memory.DL2)
    core->memory.DL1 = cache_create(core,name,CACHE_READWRITE,sets,assoc,linesize,
                             rp,ap,wp,wc,banks,bank_width,latency,
                             MSHR_entries,MSHR_WB_entries,1,core->memory.DL2,core->memory.DL2_bus);
  else
    core->memory.DL1 = cache_create(core,name,CACHE_READWRITE,sets,assoc,linesize,
                             rp,ap,wp,wc,banks,bank_width,latency,
                             MSHR_entries,MSHR_WB_entries,1,uncore->LLC,uncore->LLC_bus);
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
  for(i=0;i<knobs->memory.DL1_num_PF;i++)
    core->memory.DL1->prefetcher[i] = prefetch_create(knobs->memory.DL1PF_opt_str[i],core->memory.DL1);
  if(core->memory.DL1->prefetcher[0] == NULL)
    core->memory.DL1->num_prefetchers = knobs->memory.DL1_num_PF = 0;
  core->memory.DL1->prefetch_on_miss = knobs->memory.DL1_PF_on_miss;

  core->memory.DL1->PFF_size = knobs->memory.DL1_PFFsize;
  core->memory.DL1->PFF = (cache_t::PFF_t*) calloc(knobs->memory.DL1_PFFsize,sizeof(*core->memory.DL1->PFF));
  if(!core->memory.DL1->PFF)
    fatal("failed to calloc %s's prefetch FIFO",core->memory.DL1->name);
  core->memory.DL1->prefetch_threshold = knobs->memory.DL1_PFthresh;
  core->memory.DL1->prefetch_max = knobs->memory.DL1_PFmax;
  core->memory.DL1->PF_low_watermark = knobs->memory.DL1_low_watermark;
  core->memory.DL1->PF_high_watermark = knobs->memory.DL1_high_watermark;
  core->memory.DL1->PF_sample_interval = knobs->memory.DL1_WMinterval;

  core->memory.DL1->controller = controller_create(knobs->memory.DL1_controller_opt_str, core, core->memory.DL1);
  if(core->memory.DL2 != NULL)
    core->memory.DL2->controller = controller_create(knobs->memory.DL2_controller_opt_str, core, core->memory.DL2);

  /* DTLB2 */
  if(strcasecmp(knobs->memory.DTLB2_opt_str,"none") != 0)
    warnonce("DTLB2 not used in STM model; option string ignored");
  core->memory.DTLB_bus = NULL;
  core->memory.DTLB2 = NULL;

  /* DTLB */
  if(sscanf(knobs->memory.DTLB_opt_str,"%[^:]:%d:%d:%d:%d:%c:%d",
      name,&sets,&assoc,&banks,&latency, &rp, &MSHR_entries) != 7)
    fatal("invalid DTLB options: <name:sets:assoc:banks:latency:repl-policy:num-MSHR>");

  if(core->memory.DL2)
    core->memory.DTLB = cache_create(core,name,CACHE_READONLY,sets,assoc,1,rp,'w','t','n',banks,1,latency,MSHR_entries,4,1,core->memory.DL2,core->memory.DL2_bus);
  else
    core->memory.DTLB = cache_create(core,name,CACHE_READONLY,sets,assoc,1,rp,'w','t','n',banks,1,latency,MSHR_entries,4,1,uncore->LLC,uncore->LLC_bus);
  core->memory.DTLB->MSHR_cmd_order = NULL;

  core->memory.DTLB->controller = controller_create(knobs->memory.DTLB_controller_opt_str, core, core->memory.DTLB);
  if(core->memory.DTLB2 != NULL)
    core->memory.DTLB2->controller = controller_create(knobs->memory.DTLB2_controller_opt_str, core, core->memory.DTLB2);

  /*******************/
  /* execution ports */
  /*******************/
  port = (core_exec_STM_t::exec_port_t*) calloc(knobs->exec.num_exec_ports,sizeof(*port));
  if(!port)
    fatal("couldn't calloc exec ports");

  /***************************/
  /* execution port bindings */
  /***************************/
  for(i=0;i<NUM_FU_CLASSES;i++)
  {
    int j;
    knobs->exec.port_binding[i].ports = (int*) calloc(knobs->exec.port_binding[i].num_FUs,sizeof(int));
    if(!knobs->exec.port_binding[i].ports) fatal("couldn't calloc %s ports",MD_FU_NAME(i));
    for(j=0;j<knobs->exec.port_binding[i].num_FUs;j++)
    {
      if((knobs->exec.fu_bindings[i][j] < 0) || (knobs->exec.fu_bindings[i][j] >= knobs->exec.num_exec_ports))
        fatal("port binding for %s is negative or exceeds the execution width (should be > 0 and < %d)",MD_FU_NAME(i),knobs->exec.num_exec_ports);
      knobs->exec.port_binding[i].ports[j] = knobs->exec.fu_bindings[i][j];
    }
  }

  /***************************************/
  /* functional unit execution pipelines */
  /***************************************/
  for(i=0;i<NUM_FU_CLASSES;i++)
  {
    int j;
    for(j=0;j<knobs->exec.port_binding[i].num_FUs;j++)
    {
      int port_num = knobs->exec.port_binding[i].ports[j];
      int latency = knobs->exec.latency[i];
      int issue_rate = knobs->exec.issue_rate[i];
      int heap_size = 1<<((int)rint(ceil(log(latency+1)/log(2.0))));
      port[port_num].FU[i] = (struct ALU_t*) calloc(1,sizeof(struct ALU_t));
      if(!port[port_num].FU[i])
        fatal("couldn't calloc IEU");
      port[port_num].FU[i]->latency = latency;
      port[port_num].FU[i]->issue_rate = issue_rate;
      port[port_num].FU[i]->pipe = (struct uop_action_t*) calloc(heap_size,sizeof(struct uop_action_t));
      if(!port[port_num].FU[i]->pipe)
        fatal("couldn't calloc %s function unit execution pipeline",MD_FU_NAME(i));

      if(i==FU_LD) /* load has AGEN and STQ access pipelines */
      {
        port[port_num].STQ = (struct ALU_t*) calloc(1,sizeof(struct ALU_t));
        latency = core->memory.DL1->latency; /* assume STQ latency matched to DL1's */
        heap_size = 1<<((int)rint(ceil(log(latency+1)/log(2.0))));
        port[port_num].STQ->latency = latency;
        port[port_num].STQ->issue_rate = issue_rate;
        port[port_num].STQ->pipe = (struct uop_action_t*) calloc(heap_size,sizeof(struct uop_action_t));
        if(!port[port_num].STQ->pipe)
          fatal("couldn't calloc load's STQ exec pipe on port %d",j);
      }
    }
  }

  /* shortened list of the FU's available on each port (to speed up FU loop in ALU_exec) */
  for(i=0;i<knobs->exec.num_exec_ports;i++)
  {
    port[i].num_FU_types = 0;
    int j;
    for(j=0;j<NUM_FU_CLASSES;j++)
      if(port[i].FU[j])
        port[i].num_FU_types++;
    port[i].FU_types = (enum md_fu_class *)calloc(port[i].num_FU_types,sizeof(enum md_fu_class));
    if(!port[i].FU_types)
      fatal("couldn't calloc FU_types array on port %d",i);
    int k=0;
    for(j=0;j<NUM_FU_CLASSES;j++)
    {
      if(port[i].FU[j])
      {
        port[i].FU_types[k] = (enum md_fu_class)j;
        k++;
      }
    }
  }

  core->memory.mem_repeater = repeater_create(core->knobs->exec.repeater_opt_str, core, "MR1", core->memory.DL1);

  check_for_work = true;
}

void
core_exec_STM_t::reg_stats(struct stat_sdb_t *sdb)
{
  char buf[1024];
  char buf2[1024];
  struct thread_t * arch = core->current_thread;

  stat_reg_note(sdb,"#### EXEC STATS ####");
  sprintf(buf,"c%d.exec_uops_issued",arch->id);
  stat_reg_counter(sdb, true, buf, "number of uops issued", &core->stat.exec_uops_issued, 0, TRUE, NULL);
  sprintf(buf,"c%d.exec_uPC",arch->id);
  sprintf(buf2,"c%d.exec_uops_issued/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "average number of uops executed per cycle", buf2, NULL);
  sprintf(buf,"c%d.exec_uops_replayed",arch->id);
  stat_reg_counter(sdb, true, buf, "number of uops replayed", &core->stat.exec_uops_replayed, 0, TRUE, NULL);
  sprintf(buf,"c%d.exec_avg_replays",arch->id);
  sprintf(buf2,"c%d.exec_uops_replayed/c%d.exec_uops_issued",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "average replays per uop", buf2, NULL);
  sprintf(buf,"c%d.exec_uops_snatched",arch->id);
  stat_reg_counter(sdb, true, buf, "number of uops snatched-back", &core->stat.exec_uops_snatched_back, 0, TRUE, NULL);
  sprintf(buf,"c%d.exec_avg_snatched",arch->id);
  sprintf(buf2,"c%d.exec_uops_snatched/c%d.exec_uops_issued",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "average snatch-backs per uop", buf2, NULL);
  sprintf(buf,"c%d.num_jeclear",arch->id);
  stat_reg_counter(sdb, true, buf, "number of branch mispredictions", &core->stat.num_jeclear, 0, TRUE, NULL);
  sprintf(buf,"c%d.num_wp_jeclear",arch->id);
  stat_reg_counter(sdb, true, buf, "number of branch mispredictions in the shadow of an earlier mispred", &core->stat.num_wp_jeclear, 0, TRUE, NULL);

  sprintf(buf,"c%d.RS_occupancy",arch->id);
  stat_reg_counter(sdb, false, buf, "total RS occupancy", &core->stat.RS_occupancy, 0, TRUE, NULL);
  sprintf(buf,"c%d.RS_empty",arch->id);
  stat_reg_counter(sdb, false, buf, "total cycles RS was empty", &core->stat.RS_empty_cycles, 0, TRUE, NULL);
  sprintf(buf,"c%d.RS_full",arch->id);
  stat_reg_counter(sdb, false, buf, "total cycles RS was full", &core->stat.RS_full_cycles, 0, TRUE, NULL);
  sprintf(buf,"c%d.RS_avg",arch->id);
  sprintf(buf2,"c%d.RS_occupancy/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "average RS occupancy", buf2, NULL);
  sprintf(buf,"c%d.RS_frac_empty",arch->id);
  sprintf(buf2,"c%d.RS_empty/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "fraction of cycles RS was empty", buf2, NULL);
  sprintf(buf,"c%d.RS_frac_full",arch->id);
  sprintf(buf2,"c%d.RS_full/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "fraction of cycles RS was full", buf2, NULL);

  sprintf(buf,"c%d.LDQ_occupancy",arch->id);
  stat_reg_counter(sdb, true, buf, "total LDQ occupancy", &core->stat.LDQ_occupancy, 0, TRUE, NULL);
  sprintf(buf,"c%d.LDQ_empty",arch->id);
  stat_reg_counter(sdb, true, buf, "total cycles LDQ was empty", &core->stat.LDQ_empty_cycles, 0, TRUE, NULL);
  sprintf(buf,"c%d.LDQ_full",arch->id);
  stat_reg_counter(sdb, true, buf, "total cycles LDQ was full", &core->stat.LDQ_full_cycles, 0, TRUE, NULL);
  sprintf(buf,"c%d.LDQ_avg",arch->id);
  sprintf(buf2,"c%d.LDQ_occupancy/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "average LDQ occupancy", buf2, NULL);
  sprintf(buf,"c%d.LDQ_frac_empty",arch->id);
  sprintf(buf2,"c%d.LDQ_empty/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "fraction of cycles LDQ was empty", buf2, NULL);
  sprintf(buf,"c%d.LDQ_frac_full",arch->id);
  sprintf(buf2,"c%d.LDQ_full/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "fraction of cycles LDQ was full", buf2, NULL);

  sprintf(buf,"c%d.STQ_occupancy",arch->id);
  stat_reg_counter(sdb, true, buf, "total STQ occupancy", &core->stat.STQ_occupancy, 0, TRUE, NULL);
  sprintf(buf,"c%d.STQ_empty",arch->id);
  stat_reg_counter(sdb, true, buf, "total cycles STQ was empty", &core->stat.STQ_empty_cycles, 0, TRUE, NULL);
  sprintf(buf,"c%d.STQ_full",arch->id);
  stat_reg_counter(sdb, true, buf, "total cycles STQ was full", &core->stat.STQ_full_cycles, 0, TRUE, NULL);
  sprintf(buf,"c%d.STQ_avg",arch->id);
  sprintf(buf2,"c%d.STQ_occupancy/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "average STQ occupancy", buf2, NULL);
  sprintf(buf,"c%d.STQ_frac_empty",arch->id);
  sprintf(buf2,"c%d.STQ_empty/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "fraction of cycles STQ was empty", buf2, NULL);
  sprintf(buf,"c%d.STQ_frac_full",arch->id);
  sprintf(buf2,"c%d.STQ_full/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "fraction of cycles STQ was full", buf2, NULL);

  stat_reg_note(sdb,"\n#### DATA CACHE STATS ####");
  cache_reg_stats(sdb, core, core->memory.DL1);
  cache_reg_stats(sdb, core, core->memory.DTLB);
  if(core->memory.DL2)
  {
    cache_reg_stats(sdb, core, core->memory.DL2);
    bus_reg_stats(sdb, core, core->memory.DL2_bus);
  }
}

void core_exec_STM_t::freeze_stats(void)
{
}

void core_exec_STM_t::update_occupancy(void)
{
    /* RS */
  core->stat.RS_occupancy += RS_num;
  if(RS_num >= core->knobs->exec.RS_size)
    core->stat.RS_full_cycles++;
  if(RS_num <= 0)
    core->stat.RS_empty_cycles++;

    /* LDQ */
  core->stat.LDQ_occupancy += LDQ_num;
  if(LDQ_num >= core->knobs->exec.LDQ_size)
    core->stat.LDQ_full_cycles++;
  if(LDQ_num <= 0)
    core->stat.LDQ_empty_cycles++;

    /* STQ */
  core->stat.STQ_occupancy += STQ_num;
  if(STQ_num >= core->knobs->exec.STQ_size)
    core->stat.STQ_full_cycles++;
  if(STQ_num <= 0)
    core->stat.STQ_empty_cycles++;
}

void core_exec_STM_t::reset_execution(void)
{
  struct core_knobs_t * knobs = core->knobs;
  for(int i=0; i<knobs->exec.num_exec_ports; i++)
  {
    for(int j=0; j<NUM_FU_CLASSES; j++)
      if(port[i].FU[j])
      {
        port[i].FU[j]->when_executable = core->sim_cycle;
      }
  }
  check_for_work = true;
}

/* Functions to support dependency tracking 
   NOTE: "Ready" Queue is somewhat of a misnomer... uops are placed
   in the readyQ when all of their data-flow parents have issued.
   However, that doesn't necessarily mean that the corresponding
   inputs are "ready" due to non-unit execution latencies.  So it's
   really the Ready-And-Soon-To-Be-Ready-Queue... */
core_exec_STM_t::readyQ_node_t * core_exec_STM_t::get_readyQ_node(void)
{
  struct readyQ_node_t * p = NULL;
  if(readyQ_free_pool)
  {
    p = readyQ_free_pool;
    readyQ_free_pool = p->next;
  }
  else
  {
    p = (struct readyQ_node_t*) calloc(1,sizeof(*p));
    if(!p)
      fatal("couldn't calloc a readyQ node");
  }
  assert(p);
  p->next = NULL;
  p->when_assigned = core->sim_cycle;
#ifdef DEBUG
  readyQ_free_pool_debt++;
#endif
  return p;
}

void core_exec_STM_t::return_readyQ_node(struct readyQ_node_t * const p)
{
  assert(p);
  assert(p->uop);
  p->next = readyQ_free_pool;
  readyQ_free_pool = p;
  p->uop = NULL;
  p->when_assigned = -1;
#ifdef DEBUG
  readyQ_free_pool_debt--;
#endif
}

/* Add the uop to the corresponding readyQ (based on port binding - we maintain
   one readyQ per execution port) */
void core_exec_STM_t::insert_ready_uop(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  zesto_assert((uop->alloc.port_assignment >= 0) && (uop->alloc.port_assignment < knobs->exec.num_exec_ports),(void)0);
  zesto_assert(uop->timing.when_completed == TICK_T_MAX,(void)0);
  zesto_assert(uop->timing.when_issued == TICK_T_MAX,(void)0);
  zesto_assert(!uop->exec.in_readyQ,(void)0);

  struct readyQ_node_t ** RQ = &port[uop->alloc.port_assignment].readyQ;
  struct readyQ_node_t * new_node = get_readyQ_node();
  new_node->uop = uop;
  new_node->uop_seq = uop->decode.uop_seq;
  uop->exec.in_readyQ = true;
  uop->exec.action_id = core->new_action_id();
  new_node->action_id = uop->exec.action_id;

  if(!*RQ) /* empty ready queue */
  {
    *RQ = new_node;
    return;
  }

  struct readyQ_node_t * current = *RQ, * prev = NULL;

  /* insert in age order: first find insertion point */
  while(current && (current->uop_seq < uop->decode.uop_seq))
  {
    prev = current;
    current = current->next;
  }

  if(!prev) /* uop is oldest */
  {
    new_node->next = *RQ;
    *RQ = new_node;
  }
  else
  {
    new_node->next = current;
    prev->next = new_node;
  }
}

/*****************************/
/* MAIN SCHED/EXEC FUNCTIONS */
/*****************************/

void core_exec_STM_t::RS_schedule(void) /* for uops in the RS */
{
  struct core_knobs_t * knobs = core->knobs;
  int i;

  /* select/pick from ready instructions and send to exec ports */
  for(i=0;i<knobs->exec.num_exec_ports;i++)
  {
    struct readyQ_node_t * rq = port[i].readyQ;
    struct readyQ_node_t * prev = NULL;
    int issued = false;

    if(rq && rq->uop->timing.when_ready > core->sim_cycle)
      continue; /* nothing ready on this port */

    while(rq) /* if anyone's waiting to issue to this port */
    {
      struct uop_t * uop = rq->uop;
      enum md_fu_class FU_class = uop->decode.FU_class;

      if(uop->exec.action_id != rq->action_id) /* RQ entry has been squashed */
      {
        struct readyQ_node_t * next = rq->next;
        /* remove from readyQ */
        if(prev)
          prev->next = next;
        else
          port[i].readyQ = next;
        return_readyQ_node(rq);
        /* and go on to next node */
        rq = next;
      }
      else if((uop->timing.when_ready <= core->sim_cycle) &&
              !issued &&
              (port[i].FU[FU_class]->when_executable <= core->sim_cycle) &&
              (port[i].FU[FU_class]->occupancy < port[i].FU[FU_class]->latency))
      {
        int insert_position = port[i].FU[FU_class]->occupancy+1;
        zesto_assert(uop->alloc.port_assignment == i,(void)0);

        uop->timing.when_issued = core->sim_cycle;
        port[i].FU[FU_class]->pipe[insert_position].uop = uop;
        port[i].FU[FU_class]->pipe[insert_position].action_id = uop->exec.action_id;
        port[i].FU[FU_class]->pipe[insert_position].pipe_exit_time = core->sim_cycle + port[i].FU[FU_class]->latency;
        ALU_heap_balance(port[i].FU[FU_class]->pipe,insert_position);

        port[i].FU[FU_class]->occupancy++;
        port[i].FU[FU_class]->when_executable = core->sim_cycle + port[i].FU[FU_class]->issue_rate;
        check_for_work = true;

        struct readyQ_node_t * next = rq->next;
        /* remove from readyQ */
        if(prev)
          prev->next = next;
        else
          port[i].readyQ = next;
        return_readyQ_node(rq);
        uop->exec.in_readyQ = false;
        ZESTO_STAT(core->stat.exec_uops_issued++;)

        /* only one uop schedules from an issue port per cycle */
        issued = true;
        rq = next;
      }
      else
      {
        /* node valid, but not ready; skip over it */
        prev = rq;
        rq = rq->next;
      }
    }
  }
}

/* returns true if load is allowed to issue (or is predicted to be ok) */
bool core_exec_STM_t::check_load_issue_conditions(const struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  /* are all previous STA's known? If there's a match, is the STD ready? */
  bool regular_match = false;
  int i;
  int match_index = -1;
  int num_stores = 0; /* need this extra condition because STQ could be full with all stores older than the load we're considering */

  /* don't reissue someone who's already issued */
  zesto_assert((uop->alloc.LDQ_index >= 0) && (uop->alloc.LDQ_index < knobs->exec.LDQ_size),false);
  /* assume you've already checked this before calling check_load_issue_conditions */
  md_addr_t ld_addr = LDQ[uop->alloc.LDQ_index].uop->oracle.virt_addr;

  for(i=LDQ[uop->alloc.LDQ_index].store_color;
      ((modinc(i,knobs->exec.STQ_size)) != STQ_head) && (STQ[i].uop_seq < uop->decode.uop_seq) && (num_stores < STQ_num);
      i=moddec(i,knobs->exec.STQ_size) )
  {
    /* check addr match */
    int st_mem_size = STQ[i].mem_size;
    int ld_mem_size = uop->decode.mem_size;
    md_addr_t st_addr;
    
    if(STQ[i].addr_valid)
      st_addr = STQ[i].virt_addr; /* addr of first byte */
    else
    {
      zesto_assert(STQ[i].sta,false);
      return false; /* store-addr unknown */
    }

    zesto_assert(st_mem_size,false);
    zesto_assert(ld_mem_size,false);
    zesto_assert(uop->Mop->oracle.spec_mode || ld_addr,false);
    zesto_assert(st_addr || (STQ[i].sta && STQ[i].sta->Mop->oracle.spec_mode),false);

    if(st_addr == ld_addr) /* address match */
    {
      if((match_index == -1) && (STQ[i].addr_valid))
      {
        match_index = i;
        regular_match = true;
        if(!STQ[i].value_valid)
          return false; /* store-data unknown */
      }
    }
    else
    {
      /* no conflict here (or unhandled partial match) */
    }
    num_stores++;
  }

  /* no sta/std unknowns and no partial matches: good to go! */
  return true;
}

/* The callback functions below (after load_writeback) mark flags
   in the uop to specify the completion of each task, and only when
   both are done do we call the load-writeback function to finish
   off execution of the load. */

void core_exec_STM_t::load_writeback(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  zesto_assert((uop->alloc.LDQ_index >= 0) && (uop->alloc.LDQ_index < knobs->exec.LDQ_size),(void)0);
  if(!LDQ[uop->alloc.LDQ_index].hit_in_STQ) /* no match in STQ, so use cache value */
  {
    uop->exec.ovalue = uop->oracle.ovalue; /* XXX: just using oracle value for now */
    uop->exec.ovalue_valid = true;
    zesto_assert(uop->timing.when_completed == TICK_T_MAX,(void)0);
    uop->timing.when_completed = core->sim_cycle;
    update_last_completed(core->sim_cycle); /* for deadlock detection */
    if(uop->decode.is_ctrl && (uop->Mop->oracle.NextPC != uop->Mop->fetch.pred_NPC)) /* XXX: for RETN */
    {
      core->oracle->pipe_recover(uop->Mop,uop->Mop->oracle.NextPC);
      ZESTO_STAT(core->stat.num_jeclear++;)
      if(uop->Mop->oracle.spec_mode)
        ZESTO_STAT(core->stat.num_wp_jeclear++;)
    }

    uop->timing.when_otag_ready = core->sim_cycle;

    /* bypass output value to dependents; wake them up if appropriate. */
    struct odep_t * odep = uop->exec.odep_uop;

    while(odep)
    {
      tick_t when_ready = 0;

      zesto_assert(!odep->uop->exec.ivalue_valid[odep->op_num],(void)0);
      odep->uop->timing.when_ival_ready[odep->op_num] = core->sim_cycle;

      for(int j=0;j<MAX_IDEPS;j++)
        if(when_ready < odep->uop->timing.when_ival_ready[j])
          when_ready = odep->uop->timing.when_ival_ready[j];

      if(when_ready < TICK_T_MAX)
      {
        odep->uop->timing.when_ready = when_ready;
        zesto_assert(!odep->uop->exec.in_readyQ,(void)0);
        insert_ready_uop(odep->uop);
      }

      odep->uop->exec.ivalue_valid[odep->op_num] = true;
      if(odep->aflags) /* shouldn't happen for loads? */
      {
        warn("load modified flags?\n");
        odep->uop->exec.ivalue[odep->op_num].dw = uop->exec.oflags;
      }
      else
        odep->uop->exec.ivalue[odep->op_num] = uop->exec.ovalue;

      odep = odep->next;
    }
  }
}

void core_exec_STM_t::DL1_callback(void * const op)
{
  struct uop_t * const uop = (struct uop_t*) op;
  struct core_t * const core = uop->core; // for use with zesto-assert
  class core_exec_STM_t * E = (core_exec_STM_t*)uop->core->exec;
  if(uop->alloc.LDQ_index != -1)
  {
    zesto_assert(uop->exec.when_data_loaded == TICK_T_MAX,(void)0);
    uop->exec.when_data_loaded = core->sim_cycle;
    if((uop->exec.when_data_loaded <= core->sim_cycle) &&
       (uop->exec.when_addr_translated <= core->sim_cycle) &&
        !E->LDQ[uop->alloc.LDQ_index].hit_in_STQ) /* no match in STQ, so use cache value */
    {
      /* if load received value from STQ, it could have already
         committed by the time this gets called (esp. if we went to
         main memory) */
      E->load_writeback(uop);
    }
  }
}

void core_exec_STM_t::DTLB_callback(void * const op)
{
  struct uop_t * const uop = (struct uop_t*) op;
  struct core_t * const core = uop->core; 
  struct core_exec_STM_t * const E = (core_exec_STM_t*)core->exec;
  if(uop->alloc.LDQ_index != -1)
  {
    zesto_assert(uop->exec.when_addr_translated == TICK_T_MAX,(void)0);
    uop->exec.when_addr_translated = core->sim_cycle;
    if((uop->exec.when_data_loaded <= core->sim_cycle) &&
       (uop->exec.when_addr_translated <= core->sim_cycle) &&
        !E->LDQ[uop->alloc.LDQ_index].hit_in_STQ)
      E->load_writeback(uop);
  }
}

/* returns true if TLB translation has completed */
bool core_exec_STM_t::translated_callback(void * const op, const seq_t action_id)
{
  struct uop_t * const uop = (struct uop_t*) op;
  if((uop->exec.action_id == action_id) && (uop->alloc.LDQ_index != -1))
    return uop->exec.when_addr_translated <= uop->core->sim_cycle;
  else
    return true;
}

/* Used by the cache processing functions to recover the id of the
   uop without needing to know about the uop struct. */
seq_t core_exec_STM_t::get_uop_action_id(void * const op)
{
  struct uop_t * const uop = (struct uop_t*) op;
  return uop->exec.action_id;
}

/* process loads exiting the STQ search pipeline, update caches */
void core_exec_STM_t::LDST_exec(void)
{
  struct core_knobs_t * knobs = core->knobs;
  int i;

  /* process STQ pipes */
  for(i=0;i<knobs->exec.port_binding[FU_LD].num_FUs;i++)
  {
    int port_num = knobs->exec.port_binding[FU_LD].ports[i];
    int j;

    if( (port[port_num].STQ->occupancy > 0) &&
        (port[port_num].STQ->pipe[1].pipe_exit_time <= core->sim_cycle))
    {
      struct uop_t * uop = port[port_num].STQ->pipe[1].uop; // heap root
      zesto_assert(uop != NULL,(void)0);
      if(port[port_num].STQ->pipe[1].action_id == uop->exec.action_id)
      {
        int num_stores = 0;
        zesto_assert((uop->alloc.LDQ_index >= 0) && (uop->alloc.LDQ_index < knobs->exec.LDQ_size),(void)0);

        j=LDQ[uop->alloc.LDQ_index].store_color;

        int cond1 = STQ[j].sta != NULL;
        seq_t seq1 = (seq_t)-1, seq2 = (seq_t)-1;
        zesto_assert(j >= 0,(void)0);
        zesto_assert(j < knobs->exec.STQ_size,(void)0);
        if(j)
        {
          seq1 = (seq_t)-2;
        }
        if(cond1)
        {
          seq1 = STQ[j].sta->decode.uop_seq;
          seq2 = uop->decode.uop_seq;
        }
        int cond2 = cond1 && (seq1 < seq2);
        int cond3 = (num_stores < STQ_num);

        while(cond1 && cond2 && cond3)
        {
          md_addr_t st_addr = STQ[j].virt_addr;
          md_addr_t ld_addr = LDQ[uop->alloc.LDQ_index].virt_addr;

          if(STQ[j].addr_valid)
          {
            if(st_addr == ld_addr) /* match */
            {
              zesto_assert(uop->timing.when_completed == TICK_T_MAX,(void)0);
              if(STQ[j].value_valid)
              {
                uop->exec.ovalue = STQ[j].value;
                uop->exec.ovalue_valid = true;
                uop->timing.when_completed = core->sim_cycle;
                update_last_completed(core->sim_cycle); /* for deadlock detection */
                LDQ[uop->alloc.LDQ_index].hit_in_STQ = true;

                if(uop->decode.is_ctrl && (uop->Mop->oracle.NextPC != uop->Mop->fetch.pred_NPC)) /* for RETN */
                {
                  core->oracle->pipe_recover(uop->Mop,uop->Mop->oracle.NextPC);
                  ZESTO_STAT(core->stat.num_jeclear++;)
                  if(uop->Mop->oracle.spec_mode)
                    ZESTO_STAT(core->stat.num_wp_jeclear++;)
                }

                uop->timing.when_otag_ready = core->sim_cycle;

                struct odep_t * odep = uop->exec.odep_uop;

                while(odep)
                {
                  tick_t when_ready = 0;

                  odep->uop->timing.when_ival_ready[odep->op_num] = core->sim_cycle;

                  for(int j=0;j<MAX_IDEPS;j++)
                    if(when_ready < odep->uop->timing.when_ival_ready[j])
                      when_ready = odep->uop->timing.when_ival_ready[j];

                  if(when_ready < TICK_T_MAX)
                  {
                    odep->uop->timing.when_ready = when_ready;
                    if(!odep->uop->exec.in_readyQ)
                      insert_ready_uop(odep->uop);
                  }

                  /* bypass output value to dependents */
                  odep->uop->exec.ivalue_valid[odep->op_num] = true;
                  if(odep->aflags) /* shouldn't happen for loads? */
                  {
                    warn("load modified flags?");
                    odep->uop->exec.ivalue[odep->op_num].dw = uop->exec.oflags;
                  }
                  else
                    odep->uop->exec.ivalue[odep->op_num] = uop->exec.ovalue;

                  odep = odep->next;
                }
              }
              else /* addr match but STD unknown */
              {
                /* When_issued is still set, so LDQ won't reschedule the load.  The load
                   will wait until the STD finishes and wakes it up. */
                uop->exec.action_id = core->new_action_id();
              }
              break;
            }
            else /* no match */
            {
              /* nothing to do */
            }
          }

          num_stores++;

          j=moddec(j,knobs->exec.STQ_size); //(j-1+knobs->exec.STQ_size) % knobs->exec.STQ_size;

          cond1 = STQ[j].sta != NULL;
          cond2 = cond1 && (STQ[j].sta->decode.uop_seq < uop->decode.uop_seq);
          cond3 = (num_stores < STQ_num);
        }
      }

      ALU_heap_remove(port[port_num].STQ->pipe,port[port_num].STQ->occupancy);
      port[port_num].STQ->occupancy--;
      zesto_assert(port[port_num].STQ->occupancy >= 0,(void)0);
    }
  }

  lk_lock(&cache_lock, core->id+1);
  if(core->memory.DTLB->check_for_work) cache_process(core->memory.DTLB);
  if(core->memory.DL2 && core->memory.DL2->check_for_work) cache_process(core->memory.DL2);
  if(core->memory.DL1->check_for_work) cache_process(core->memory.DL1);
  lk_unlock(&cache_lock);
}

/* Schedule load uops to execute from the LDQ.  Load execution occurs in a two-step
   process: first the address gets computed (issuing from the RS), and then the
   cache/STQ search occur (issuing from the LDQ). */
void core_exec_STM_t::LDQ_schedule(void)
{
  struct core_knobs_t * knobs = core->knobs;
  int i;
  int index = LDQ_head;
  /* walk LDQ, if anyone's ready, issue to DTLB/DL1 */
  for(i=0;i<LDQ_num;i++)
  {
    //int index = (LDQ_head + i) % knobs->exec.LDQ_size; // incremented at bottom of loop
    if(LDQ[index].addr_valid) /* agen has finished */
    {
      if(LDQ[index].when_issued == TICK_T_MAX) /* load hasn't issued to DL1/STQ already */
      {
        if(check_load_issue_conditions(LDQ[index].uop)) /* retval of true means load is predicted to be cleared for issue */
        {
          struct uop_t * uop = LDQ[index].uop;
          if((cache_enqueuable(core->memory.DTLB,core->current_thread->id,PAGE_TABLE_ADDR(core->current_thread->id,uop->oracle.virt_addr))) &&
             (cache_enqueuable(core->memory.DL1,core->current_thread->id,uop->oracle.virt_addr)) &&
             (port[uop->alloc.port_assignment].STQ->occupancy < port[uop->alloc.port_assignment].STQ->latency))
          {
            uop->exec.when_data_loaded = TICK_T_MAX;
            uop->exec.when_addr_translated = TICK_T_MAX;
            cache_enqueue(core,core->memory.DTLB,NULL,CACHE_READ,core->current_thread->id,uop->Mop->fetch.PC,PAGE_TABLE_ADDR(core->current_thread->id,uop->oracle.virt_addr),uop->exec.action_id,0,NO_MSHR,uop,DTLB_callback,NULL,NULL,get_uop_action_id);
            cache_enqueue(core,core->memory.DL1,NULL,CACHE_READ,core->current_thread->id,uop->Mop->fetch.PC,uop->oracle.virt_addr,uop->exec.action_id,0,NO_MSHR,uop,DL1_callback,NULL,translated_callback,get_uop_action_id);

            int insert_position = port[uop->alloc.port_assignment].STQ->occupancy+1;
            port[uop->alloc.port_assignment].STQ->pipe[insert_position].uop = uop;
            port[uop->alloc.port_assignment].STQ->pipe[insert_position].action_id = uop->exec.action_id;
            port[uop->alloc.port_assignment].STQ->pipe[insert_position].pipe_exit_time = core->sim_cycle + port[uop->alloc.port_assignment].STQ->latency;
            ALU_heap_balance(port[uop->alloc.port_assignment].STQ->pipe,insert_position);
            port[uop->alloc.port_assignment].STQ->occupancy++;
            LDQ[index].when_issued = core->sim_cycle;
          }
        }
        else
        {
          LDQ[index].uop->timing.when_otag_ready = TICK_T_MAX;
          LDQ[index].uop->timing.when_completed = TICK_T_MAX;
          LDQ[index].hit_in_STQ = false;
        }
      }
    }
    index = modinc(index,knobs->exec.LDQ_size);
  }
}

/* Process execution (in ALUs) of uops */
void core_exec_STM_t::ALU_exec(void)
{
  struct core_knobs_t * knobs = core->knobs;
  int i;

  if(check_for_work == false)
    return;

  bool work_found = false;

  /* Process Functional Units */
  for(i=0;i<knobs->exec.num_exec_ports;i++)
  {
    int j;
    for(j=0;j<port[i].num_FU_types;j++)
    {
      enum md_fu_class FU_type = port[i].FU_types[j];
      struct ALU_t * FU = port[i].FU[FU_type];
      if(FU && (FU->occupancy > 0))
      {
        work_found = true;
        /* process last stage of FU pipeline (those uops completing execution) */
        if(FU->pipe[1].pipe_exit_time <= core->sim_cycle) // heap root
        {
          struct uop_t * uop = FU->pipe[1].uop;
          zesto_assert(uop != NULL,(void)0);

          int squashed = (FU->pipe[1].action_id != uop->exec.action_id);
          ALU_heap_remove(FU->pipe,FU->occupancy);

          FU->occupancy--;
          zesto_assert(FU->occupancy >= 0,(void)0);

          /* there's a uop completing execution (that hasn't been squashed) */
          if(!squashed)
          {
            if(uop->decode.is_load) /* loads need to be processed differently */
            {
              /* update load queue entry */
              zesto_assert((uop->alloc.LDQ_index >= 0) && (uop->alloc.LDQ_index < knobs->exec.LDQ_size),(void)0);
              LDQ[uop->alloc.LDQ_index].virt_addr = uop->oracle.virt_addr;
              LDQ[uop->alloc.LDQ_index].addr_valid = true;
              /* actual scheduling from load queue takes place in LDQ_schedule() */
            }
            else
            {
              /* XXX for now just copy oracle value */
              uop->exec.ovalue_valid = true;
              uop->exec.ovalue = uop->oracle.ovalue;

              /* alloc, uopQ, and decode all have to search for the
                 recovery point (Mop) because a long flow may have
                 uops that span multiple sections of the pipeline.  */
              if(uop->decode.is_ctrl && (uop->Mop->oracle.NextPC != uop->Mop->fetch.pred_NPC))
              {
                /* XXX: should compute */
                core->oracle->pipe_recover(uop->Mop,uop->Mop->oracle.NextPC);
                ZESTO_STAT(core->stat.num_jeclear++;)
                if(uop->Mop->oracle.spec_mode)
                  ZESTO_STAT(core->stat.num_wp_jeclear++;)
              }
              else if(uop->decode.is_sta)
              {
                zesto_assert((uop->alloc.STQ_index >= 0) && (uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
                zesto_assert(!STQ[uop->alloc.STQ_index].addr_valid,(void)0);
                STQ[uop->alloc.STQ_index].virt_addr = uop->oracle.virt_addr;
                STQ[uop->alloc.STQ_index].addr_valid = true;
              }
              else if(uop->decode.is_std)
              {
                zesto_assert((uop->alloc.STQ_index >= 0) && (uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
                zesto_assert(!STQ[uop->alloc.STQ_index].value_valid,(void)0);
                STQ[uop->alloc.STQ_index].value = uop->exec.ovalue;
                STQ[uop->alloc.STQ_index].value_valid = true;
              }

              if((uop->decode.is_sta || uop->decode.is_std) &&
                  STQ[uop->alloc.STQ_index].addr_valid &&
                  STQ[uop->alloc.STQ_index].value_valid)
              {
                /* both parts of store have now made it to the STQ: walk
                   forward in LDQ to see if there are any loads waiting on this
                   this STD.  If so, unblock them. */
                int idx;
                int num_loads = 0;
                int overwrite_index = uop->alloc.STQ_index;

                /* XXX using oracle info here. */
                zesto_assert((uop->alloc.STQ_index >= 0) && (uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
                md_addr_t st_addr = STQ[uop->alloc.STQ_index].sta->oracle.virt_addr;

                for(idx=STQ[uop->alloc.STQ_index].next_load;
                    LDQ[idx].uop && (LDQ[idx].uop->decode.uop_seq > uop->decode.uop_seq) && (num_loads < LDQ_num);
                    idx=modinc(idx,knobs->exec.LDQ_size))
                {
                  if(LDQ[idx].store_color != uop->alloc.STQ_index) /* some younger stores present */
                  {
                    bool overwritten = false;
                    /* scan store queue for younger loads to see if we've been overwritten */
                    while(overwrite_index != LDQ[idx].store_color)
                    {
                      overwrite_index = modinc(overwrite_index,knobs->exec.STQ_size);
                      if(overwrite_index == STQ_tail)
                        zesto_fatal("searching for matching store color but hit the end of the STQ",(void)0);

                      md_addr_t new_st_addr = STQ[overwrite_index].virt_addr;

                      /* does this store overwrite any of our bytes? 
                         does not properly handle partial forwarding cases */
                      if(STQ[overwrite_index].addr_valid && (st_addr == new_st_addr))
                      {
                        overwritten = true;
                        break; /* while */
                      }
                    }

                    if(overwritten)
                      break; /* for */
                  }

                  if(LDQ[idx].addr_valid && /* if addr not valid, load hasn't finished AGEN... it'll pick up the right value after AGEN */
                     (LDQ[idx].when_issued != TICK_T_MAX)) /* similar to addr not valid, the load'll grab the value when it issues from the LDQ */
                  {
                    md_addr_t ld_addr = LDQ[idx].virt_addr;

                    if(st_addr == ld_addr)
                    {
                      /* possible that the load already executed due to imprecise
                         tracking of partial forwarding, multiple stores to the
                         same address, etc. */
                      if(LDQ[idx].uop->timing.when_completed == TICK_T_MAX)
                      {
                        /* mark load as schedulable */
                        LDQ[idx].when_issued = TICK_T_MAX; /* this will allow it to get issued from LDQ_schedule */
                        LDQ[idx].hit_in_STQ = false; /* it's possible the store commits before the load gets back to searching the STQ */

                        /* invalidate any in-flight loads from cache hierarchy */
                        LDQ[idx].uop->exec.action_id = core->new_action_id();
                      }
                    }
                  }

                  num_loads++;
                }
              }

              zesto_assert(uop->timing.when_completed == TICK_T_MAX,(void)0);
              uop->timing.when_completed = core->sim_cycle;
              update_last_completed(core->sim_cycle); /* for deadlock detection */

              /* bypass output value to dependents */
              struct odep_t * odep = uop->exec.odep_uop;
              while(odep)
              {
                tick_t when_ready = 0;

                odep->uop->timing.when_ival_ready[odep->op_num] = core->sim_cycle;

                for(int j=0;j<MAX_IDEPS;j++)
                  if(when_ready < odep->uop->timing.when_ival_ready[j])
                    when_ready = odep->uop->timing.when_ival_ready[j];

                if(when_ready < TICK_T_MAX)
                {
                  odep->uop->timing.when_ready = when_ready;
                  if(!odep->uop->exec.in_readyQ)
                    insert_ready_uop(odep->uop);
                }

                zesto_assert(!odep->uop->exec.ivalue_valid[odep->op_num],(void)0);
                odep->uop->exec.ivalue_valid[odep->op_num] = true;
                if(odep->aflags)
                  odep->uop->exec.ivalue[odep->op_num].dw = uop->exec.oflags;
                else
                  odep->uop->exec.ivalue[odep->op_num] = uop->exec.ovalue;

                odep = odep->next;
              }
            }

            /* deallocate the RS entry */
            core->exec->RS_deallocate(uop);
            /* update port loading table */
            core->alloc->RS_deallocate(uop);
          }
        }
      }
    }
  }

  check_for_work = work_found;
}

void core_exec_STM_t::recover(const struct Mop_t * const Mop)
{
  /* most flushing/squashing is accomplished through assignments of new action_id's */
}

void core_exec_STM_t::recover(void)
{
  /* most flushing/squashing is accomplished through assignments of new action_id's */
}

bool core_exec_STM_t::RS_available(void)
{
  struct core_knobs_t * knobs = core->knobs;
  return RS_num < knobs->exec.RS_size;
}

/* assumes you already called RS_available to check that
   an entry is available */
void core_exec_STM_t::RS_insert(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  int RS_index;
  /* find a free RS entry */
  for(RS_index=0;RS_index < knobs->exec.RS_size;RS_index++)
  {
    if(RS[RS_index] == NULL)
      break;
  }
  if(RS_index == knobs->exec.RS_size)
    zesto_fatal("RS and RS_num out of sync",(void)0);

  RS[RS_index] = uop;
  RS_num++;
  uop->alloc.RS_index = RS_index;
}

/* add uops to an existing entry (where all uops in the same
   entry are assumed to be fused) */
void core_exec_STM_t::RS_fuse_insert(struct uop_t * const uop)
{
  fatal("fusion not supported in STM exec module");
}

void core_exec_STM_t::RS_deallocate(struct uop_t * const dead_uop)
{
  struct core_knobs_t * knobs = core->knobs;
  zesto_assert(dead_uop->alloc.RS_index < knobs->exec.RS_size,(void)0);

  RS[dead_uop->alloc.RS_index] = NULL;
  RS_num --;
  zesto_assert(RS_num >= 0,(void)0);
  dead_uop->alloc.RS_index = -1;
}

bool core_exec_STM_t::LDQ_available(void)
{
  struct core_knobs_t * knobs = core->knobs;
  return LDQ_num < knobs->exec.LDQ_size;
}

void core_exec_STM_t::LDQ_insert(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  //memset(&LDQ[LDQ_tail],0,sizeof(*LDQ));
  memzero(&LDQ[LDQ_tail],sizeof(*LDQ));
  LDQ[LDQ_tail].uop = uop;
  LDQ[LDQ_tail].mem_size = uop->decode.mem_size;
  LDQ[LDQ_tail].store_color = moddec(STQ_tail,knobs->exec.STQ_size); //(STQ_tail - 1 + knobs->exec.STQ_size) % knobs->exec.STQ_size;
  LDQ[LDQ_tail].when_issued = TICK_T_MAX;
  uop->alloc.LDQ_index = LDQ_tail;
  LDQ_num++;
  LDQ_tail = modinc(LDQ_tail,knobs->exec.LDQ_size); //(LDQ_tail+1) % knobs->exec.LDQ_size;
}

/* called by commit */
void core_exec_STM_t::LDQ_deallocate(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  LDQ[LDQ_head].uop = NULL;
  LDQ_num --;
  LDQ_head = modinc(LDQ_head,knobs->exec.LDQ_size); //(LDQ_head+1) % knobs->exec.LDQ_size;
  uop->alloc.LDQ_index = -1;
}

void core_exec_STM_t::LDQ_squash(struct uop_t * const dead_uop)
{
  struct core_knobs_t * knobs = core->knobs;
  zesto_assert((dead_uop->alloc.LDQ_index >= 0) && (dead_uop->alloc.LDQ_index < knobs->exec.LDQ_size),(void)0);
  zesto_assert(LDQ[dead_uop->alloc.LDQ_index].uop == dead_uop,(void)0);
  //memset(&LDQ[dead_uop->alloc.LDQ_index],0,sizeof(LDQ[0]));
  memzero(&LDQ[dead_uop->alloc.LDQ_index],sizeof(LDQ[0]));
  LDQ_num --;
  LDQ_tail = moddec(LDQ_tail,knobs->exec.LDQ_size); //(LDQ_tail - 1 + knobs->exec.LDQ_size) % knobs->exec.LDQ_size;
  zesto_assert(LDQ_num >= 0,(void)0);
  dead_uop->alloc.LDQ_index = -1;
}

bool core_exec_STM_t::STQ_available(void)
{
  struct core_knobs_t * knobs = core->knobs;
  return STQ_num < knobs->exec.STQ_size;
}

void core_exec_STM_t::STQ_insert_sta(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  //memset(&STQ[STQ_tail],0,sizeof(*STQ));
  memzero(&STQ[STQ_tail],sizeof(*STQ));
  STQ[STQ_tail].sta = uop;
  if(STQ[STQ_tail].sta != NULL)
    uop->decode.is_sta = true;
  STQ[STQ_tail].mem_size = uop->decode.mem_size;
  STQ[STQ_tail].uop_seq = uop->decode.uop_seq;
  STQ[STQ_tail].next_load = LDQ_tail;
  uop->alloc.STQ_index = STQ_tail;
  STQ_num++;
  STQ_tail = modinc(STQ_tail,knobs->exec.STQ_size); //(STQ_tail+1) % knobs->exec.STQ_size;
}

void core_exec_STM_t::STQ_insert_std(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  /* STQ_tail already incremented from the STA.  Just add this uop to STQ->std */
  int index = moddec(STQ_tail,knobs->exec.STQ_size); //(STQ_tail - 1 + knobs->exec.STQ_size) % knobs->exec.STQ_size;
  uop->alloc.STQ_index = index;
  STQ[index].std = uop;
  zesto_assert(STQ[index].sta,(void)0); /* shouldn't have STD w/o a corresponding STA */
  zesto_assert(STQ[index].sta->Mop == uop->Mop,(void)0); /* and we should be from the same macro */
}

void core_exec_STM_t::STQ_deallocate_sta(void)
{
  STQ[STQ_head].sta = NULL;
}

/* returns true if successful */
bool core_exec_STM_t::STQ_deallocate_std(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  /* Store write back occurs here at commit. */
  if(!cache_enqueuable(core->memory.DL1,core->current_thread->id,uop->oracle.virt_addr) ||
     !cache_enqueuable(core->memory.DTLB,core->current_thread->id,PAGE_TABLE_ADDR(core->current_thread->id,uop->oracle.virt_addr)))
    return false;

  /* These are just dummy placeholders, but we need them
     because the original uop pointers will be made invalid
     when the Mop commits and the oracle reclaims the
     original uop-arrays.  The store continues to occupy STQ
     entries until the store actually finishes accessing
     memory, but commit can proceed past this store once the
     request has entered into the cache hierarchy. */
  struct uop_t * dl1_uop = core->get_uop_array(1);
  struct uop_t * dtlb_uop = core->get_uop_array(1);
  dl1_uop->core = core;
  dtlb_uop->core = core;

  dl1_uop->alloc.STQ_index = uop->alloc.STQ_index;
  dtlb_uop->alloc.STQ_index = uop->alloc.STQ_index;

  STQ[STQ_head].action_id = core->new_action_id();
  dl1_uop->exec.action_id = STQ[STQ_head].action_id;
  dtlb_uop->exec.action_id = dl1_uop->exec.action_id;

  cache_enqueue(core,core->memory.DTLB,NULL,CACHE_READ,core->current_thread->id,uop->Mop->fetch.PC,PAGE_TABLE_ADDR(core->current_thread->id,uop->oracle.virt_addr),dtlb_uop->exec.action_id,0,NO_MSHR,dtlb_uop,store_dtlb_callback,NULL,NULL,get_uop_action_id);
  cache_enqueue(core,core->memory.DL1,NULL,CACHE_WRITE,core->current_thread->id,uop->Mop->fetch.PC,uop->oracle.virt_addr,dl1_uop->exec.action_id,0,NO_MSHR,dl1_uop,store_dl1_callback,NULL,store_translated_callback,get_uop_action_id);

  STQ[STQ_head].std = NULL;
  STQ[STQ_head].translation_complete = false;
  STQ[STQ_head].write_complete = false;
  STQ_num --;
  STQ_head = modinc(STQ_head,knobs->exec.STQ_size); //(STQ_head+1) % knobs->exec.STQ_size;

  return true;
}

void core_exec_STM_t::STQ_deallocate_senior(void)
{
  fatal("Senior STQ not supported in STM exec module");
}

void core_exec_STM_t::STQ_squash_sta(struct uop_t * const dead_uop)
{
  struct core_knobs_t * knobs = core->knobs;
  zesto_assert((dead_uop->alloc.STQ_index >= 0) && (dead_uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
  zesto_assert(STQ[dead_uop->alloc.STQ_index].std == NULL,(void)0);
  zesto_assert(STQ[dead_uop->alloc.STQ_index].sta == dead_uop,(void)0);
  //memset(&STQ[dead_uop->alloc.STQ_index],0,sizeof(STQ[0]));
  memzero(&STQ[dead_uop->alloc.STQ_index],sizeof(STQ[0]));
  STQ_num --;
  STQ_tail = moddec(STQ_tail,knobs->exec.STQ_size); //(STQ_tail - 1 + knobs->exec.STQ_size) % knobs->exec.STQ_size;
  zesto_assert(STQ_num >= 0,(void)0);
  dead_uop->alloc.STQ_index = -1;
}

void core_exec_STM_t::STQ_squash_std(struct uop_t * const dead_uop)
{
  struct core_knobs_t * knobs = core->knobs;
  zesto_assert((dead_uop->alloc.STQ_index >= 0) && (dead_uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
  zesto_assert(STQ[dead_uop->alloc.STQ_index].std == dead_uop,(void)0);
  STQ[dead_uop->alloc.STQ_index].std = NULL;
  dead_uop->alloc.STQ_index = -1;
}

void core_exec_STM_t::STQ_squash_senior(void)
{
  fatal("Senior STQ not supported in STM exec module");
}

void core_exec_STM_t::recover_check_assertions(void)
{
  zesto_assert(STQ_num == 0,(void)0);
  zesto_assert(LDQ_num == 0,(void)0);
}

/* Stores don't write back to cache/memory until commit.  When D$
   and DTLB accesses complete, these functions get called which
   update the status of the corresponding STQ entries.  The STQ
   entry cannot be deallocated until the store has completed. */
void core_exec_STM_t::store_dl1_callback(void * const op)
{
  struct uop_t * const uop = (struct uop_t *)op;
  struct core_t * const core = uop->core;
  struct core_knobs_t * knobs = core->knobs;

  zesto_assert((uop->alloc.STQ_index >= 0) && (uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
  core->return_uop_array(uop);
}

void core_exec_STM_t::store_dtlb_callback(void * const op)
{
  struct uop_t * const uop = (struct uop_t *)op;
  struct core_t * const core = uop->core;
  struct core_knobs_t * knobs = core->knobs;

  zesto_assert((uop->alloc.STQ_index >= 0) && (uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
  core->return_uop_array(uop);
}

bool core_exec_STM_t::store_translated_callback(void * const op, const seq_t action_id /* ignored */)
{
  return true;
}

/* input state: assumes that the new node has just been inserted at
   insert_position and all remaining nodes obey the heap property */
void core_exec_STM_t::ALU_heap_balance(struct uop_action_t * const pipe, const int insert_position)
{
  int pos = insert_position;
  while(pos > 1)
  {
    int parent = pos >> 1;
    if(pipe[parent].pipe_exit_time > pipe[pos].pipe_exit_time)
    {
      memswap(&pipe[parent],&pipe[pos],sizeof(*pipe));
      pos = parent;
    }
    else
      return;
  }
}

/* input state: pipe_num is the number of elements in the heap
   prior to removing the root node. */
void core_exec_STM_t::ALU_heap_remove(struct uop_action_t * const pipe, const int pipe_num)
{
  if(pipe_num == 1) /* only one node to remove */
  {
    pipe[1].uop = NULL;
    pipe[1].pipe_exit_time = TICK_T_MAX;
    return;
  }

  pipe[1] = pipe[pipe_num]; /* move last node to root */

  /* delete previous last node */
  pipe[pipe_num].uop = NULL;
  pipe[pipe_num].pipe_exit_time = TICK_T_MAX;

  /* push node down until heap property re-established */
  int pos = 1;
  while(1)
  {
    int Lindex = pos<<1;     // index of left child
    int Rindex = (pos<<1)+1; // index of right child
    tick_t myValue = pipe[pos].pipe_exit_time;

    tick_t Lvalue = TICK_T_MAX;
    tick_t Rvalue = TICK_T_MAX;
    if(Lindex < pipe_num) /* valid index */
      Lvalue = pipe[Lindex].pipe_exit_time;
    if(Rindex < pipe_num) /* valid index */
      Rvalue = pipe[Rindex].pipe_exit_time;

    if(((myValue > Lvalue) && (Lvalue != TICK_T_MAX)) ||
       ((myValue > Rvalue) && (Rvalue != TICK_T_MAX)))
    {
      if(Rvalue == TICK_T_MAX) /* swap pos with L */
      {
        memswap(&pipe[Lindex],&pipe[pos],sizeof(*pipe));
        return;
      }
      else if((myValue > Lvalue) && (Lvalue > Rvalue))
      {
        memswap(&pipe[Lindex],&pipe[pos],sizeof(*pipe));
        pos = Lindex;
      }
      else /* swap pos with R */
      {
        memswap(&pipe[Rindex],&pipe[pos],sizeof(*pipe));
        pos = Rindex;
      }
    }
    else
      return;
  }
}

void core_exec_STM_t::step()
{
  /* Compatibility: Simulation can call this */
}
        
void core_exec_STM_t::exec_fuse_insert(struct uop_t * uop)
{       
  fatal("shouldn't be called");
}

void core_exec_STM_t::exec_insert(struct uop_t * uop)
{
  fatal("shouldn't be called");
}

bool core_exec_STM_t::port_available(int port_ind)
{      
  fatal("shouldn't be called");
}

bool core_exec_STM_t::exec_fused_ST(struct uop_t * const curr_uop)
{
 fatal("shouldn't be called");
}

bool core_exec_STM_t::exec_empty()
{
  fatal("shouldn't be called");
}
#endif
