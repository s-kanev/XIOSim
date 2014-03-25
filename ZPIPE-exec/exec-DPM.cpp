/* exec-DPM.cpp - Detailed Pipeline Model */
/*
 * __COPYRIGHT__ GT
 */


#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(exec_opt_string,"DPM"))
    return new core_exec_DPM_t(core);
#else

class core_exec_DPM_t:public core_exec_t
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
  };

  /* struct for a generic pipelined ALU */
  struct ALU_t {
    struct uop_action_t * pipe;
    int occupancy;
    int latency;    /* number of cycles from start of execution to end */
    int issue_rate; /* number of cycles between issuing independent instructions on this ALU */
    tick_t when_scheduleable; /* cycle when next instruction can be scheduled for this ALU */
    tick_t when_executable; /* cycle when next instruction can actually start executing on this ALU */
  };

  public:

  core_exec_DPM_t(struct core_t * const core);
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

  virtual bool STQ_empty(void);
  virtual bool STQ_available(void);
  virtual void STQ_insert_sta(struct uop_t * const uop);
  virtual void STQ_insert_std(struct uop_t * const uop);
  virtual void STQ_deallocate_sta(void);
  virtual bool STQ_deallocate_std(struct uop_t * const uop);
  virtual void STQ_deallocate_senior(void);
  virtual void STQ_squash_sta(struct uop_t * const dead_uop);
  virtual void STQ_squash_std(struct uop_t * const dead_uop);
  virtual void STQ_squash_senior(void);
  virtual void STQ_set_addr(struct uop_t * const uop);
  virtual void STQ_set_data(struct uop_t * const uop);

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
  int RS_eff_num;

  struct LDQ_t {
    struct uop_t * uop;
    md_addr_t virt_addr;
    md_paddr_t phys_addr;
    bool first_byte_requested;
    bool last_byte_requested;
    bool first_byte_arrived;
    bool last_byte_arrived;
    bool repeater_first_arrived;
    bool repeater_last_arrived;
    bool first_repeated;
    bool last_repeated;
    int mem_size;
    bool addr_valid;
    int store_color;   /* STQ index of most recent store before this load */
    seq_t colored_store_action_id; /* and epoch of that store for fences */
    bool hit_in_STQ;    /* received value from STQ */
    tick_t when_issued; /* when load actually issued */
    bool speculative_broadcast; /* made a speculative tag broadcast */
    bool partial_forward; /* true if blocked on an earlier partially matching store */
    bool all_arrived(void) {
      return ((this->first_repeated && this->repeater_first_arrived) ||
               (!this->first_repeated && this->first_byte_arrived)) &&
              ((this->last_repeated && this->repeater_last_arrived) ||
               (!this->last_repeated && this->last_byte_arrived));
    }
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
    bool first_byte_requested;
    bool last_byte_requested;
    bool first_byte_written;
    bool last_byte_written;
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
  int STQ_senior_num;
  int STQ_senior_head;
  bool partial_forward_throttle; /* used to control load-issuing in the presence of partial-matching stores */

  struct exec_port_t {
    struct uop_action_t * payload_pipe;
    int occupancy;
    struct ALU_t * FU[NUM_FU_CLASSES];
    struct readyQ_node_t * readyQ;
    struct ALU_t * STQ; /* store-queue lookup/search pipeline for load execution */
    tick_t when_bypass_used; /* to make sure only one inst writes back per cycle, which
                                could happen due to insts with different latencies */
    int num_FU_types; /* the number of FU's bound to this port */
    enum md_fu_class * FU_types; /* the corresponding types of those FUs */
  } * port;
  bool check_for_work; /* used to skip ALU exec when there's no uops */

  struct memdep_t * memdep;


  /* various exec utility functions */

  struct readyQ_node_t * get_readyQ_node(void);
  void return_readyQ_node(struct readyQ_node_t * const p);
  bool check_load_issue_conditions(const struct uop_t * const uop);
  void snatch_back(struct uop_t * const replayed_uop);

  void load_writeback(struct uop_t * const uop);
  void ST_ALU_exec(const struct uop_t * const uop);
  bool is_senior_STQ_entry_valid(int STQ_ind);

  /* callbacks need to be static */
  static void DL1_callback(void * const op);
  static void DL1_split_callback(void * const op);
  static void DTLB_callback(void * const op);
  static bool translated_callback(void * const op,const seq_t);
  static seq_t get_uop_action_id(void * const op);
  static void load_miss_reschedule(void * const op, const int new_pred_latency);

  /* callbacks used by commit for stores */
  static void store_dl1_callback(void * const op);
  static void store_dl1_split_callback(void * const op);
  static void store_dtlb_callback(void * const op);
  static bool store_translated_callback(void * const op,const seq_t);


  /* callbacks used by memory repeater */
  static void repeater_callback(void * const op, bool is_hit);
  static void repeater_split_callback(void * const op, bool is_hit);
  static void repeater_store_callback(void * const op, bool is_hit);
  static void repeater_split_store_callback(void * const op, bool is_hit);
};

/*******************/
/* SETUP FUNCTIONS */
/*******************/

core_exec_DPM_t::core_exec_DPM_t(struct core_t * const arg_core):
  readyQ_free_pool(NULL),
#ifdef DEBUG
  readyQ_free_pool_debt(0),
#endif
  RS_num(0), RS_eff_num(0), LDQ_head(0), LDQ_tail(0), LDQ_num(0),
  STQ_head(0), STQ_tail(0), STQ_num(0), STQ_senior_num(0),
  STQ_senior_head(0), partial_forward_throttle(false)
{
  struct core_knobs_t * knobs = arg_core->knobs;
  core = arg_core;

  RS = (struct uop_t**) calloc(knobs->exec.RS_size,sizeof(*RS));
  if(!RS)
    fatal("couldn't calloc RS");

  LDQ = (core_exec_DPM_t::LDQ_t*) calloc(knobs->exec.LDQ_size,sizeof(*LDQ));
  if(!LDQ)
    fatal("couldn't calloc LDQ");

  STQ = (core_exec_DPM_t::STQ_t*) calloc(knobs->exec.STQ_size,sizeof(*STQ));
  if(!STQ)
    fatal("couldn't calloc STQ");

  int i;
  /* This shouldn't be necessary, but I threw it in because valgrind (memcheck)
     was reporting that STQ[i].sta was being used uninitialized. -GL */
  for(i=0;i<knobs->exec.STQ_size;i++)
    STQ[i].sta = NULL;

  /*********************************************************/
  /* Data cache parsing first, functional units after this */
  /*********************************************************/
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

    core->memory.DL2_bus = bus_create("DL2_bus", core->memory.DL2->linesize, &core->memory.DL2->sim_cycle, 1);
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


  /* DTLBs */

  /* DTLB2 */
  if(!strcasecmp(knobs->memory.DTLB2_opt_str,"none"))
  {
    core->memory.DTLB2 = NULL;
    core->memory.DTLB_bus = NULL;
  }
  else
  {
    core->memory.DTLB_bus = bus_create("DTLB_bus", 1, &core->sim_cycle, 1);

    if(sscanf(knobs->memory.DTLB2_opt_str,"%[^:]:%d:%d:%d:%d:%c:%d",
        name,&sets,&assoc,&banks,&latency, &rp, &MSHR_entries) != 7)
      fatal("invalid DTLB2 options: <name:sets:assoc:banks:latency:repl-policy:num-MSHR>");

    if(core->memory.DL2)
      core->memory.DTLB2 = cache_create(core,name,CACHE_READONLY,sets,assoc,1,rp,'w','t','n',banks,1,latency,MSHR_entries,4,1,core->memory.DL2,core->memory.DL2_bus); /* on a complete TLB miss, go to the L2 cache to simulate the traffic from a HW page-table walker */
    else
      core->memory.DTLB2 = cache_create(core,name,CACHE_READONLY,sets,assoc,1,rp,'w','t','n',banks,1,latency,MSHR_entries,4,1,uncore->LLC,uncore->LLC_bus); /* on a complete TLB miss, go to the LLC to simulate the traffic from a HW page-table walker */
    core->memory.DTLB2->MSHR_cmd_order = NULL;
  }

  /* DTLB */
  if(sscanf(knobs->memory.DTLB_opt_str,"%[^:]:%d:%d:%d:%d:%c:%d",
      name,&sets,&assoc,&banks,&latency, &rp, &MSHR_entries) != 7)
    fatal("invalid DTLB options: <name:sets:assoc:banks:latency:repl-policy:num-MSHR>");

  if(core->memory.DTLB2)
  {
    core->memory.DTLB = cache_create(core,name,CACHE_READONLY,sets,assoc,1,rp,'w','t','n',banks,1,latency,MSHR_entries,4,1,core->memory.DTLB2,core->memory.DTLB_bus);
    core->memory.DTLB->MSHR_cmd_order = NULL;
  }
  else
  {
    core->memory.DTLB = cache_create(core,name,CACHE_READONLY,sets,assoc,1,rp,'w','t','n',banks,1,latency,MSHR_entries,4,1,uncore->LLC,uncore->LLC_bus);
    core->memory.DTLB->MSHR_cmd_order = NULL;
  }

  core->memory.DTLB->controller = controller_create(knobs->memory.DTLB_controller_opt_str, core, core->memory.DTLB);
  if(core->memory.DTLB2 != NULL)
    core->memory.DTLB2->controller = controller_create(knobs->memory.DTLB2_controller_opt_str, core, core->memory.DTLB2);


  /************************************/
  /* execution port payload pipelines */
  /************************************/
  port = (core_exec_DPM_t::exec_port_t*) calloc(knobs->exec.num_exec_ports,sizeof(*port));
  if(!port)
    fatal("couldn't calloc exec ports");
  for(i=0;i<knobs->exec.num_exec_ports;i++)
  {
    port[i].payload_pipe = (struct uop_action_t*) calloc(knobs->exec.payload_depth,sizeof(*port->payload_pipe));
    if(!port[i].payload_pipe)
      fatal("couldn't calloc payload pipe");
  }

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
      port[port_num].FU[i] = (struct ALU_t*) calloc(1,sizeof(struct ALU_t));
      if(!port[port_num].FU[i])
        fatal("couldn't calloc IEU");
      port[port_num].FU[i]->latency = latency;
      port[port_num].FU[i]->issue_rate = issue_rate;
      port[port_num].FU[i]->pipe = (struct uop_action_t*) calloc(latency,sizeof(struct uop_action_t));
      if(!port[port_num].FU[i]->pipe)
        fatal("couldn't calloc %s function unit execution pipeline",MD_FU_NAME(i));

      if(i==FU_LD) /* load has AGEN and STQ access pipelines */
      {
        port[port_num].STQ = (struct ALU_t*) calloc(1,sizeof(struct ALU_t));
        latency = core->memory.DL1->latency; /* assume STQ latency matched to DL1's */
        port[port_num].STQ->latency = latency;
        port[port_num].STQ->issue_rate = issue_rate;
        port[port_num].STQ->pipe = (struct uop_action_t*) calloc(latency,sizeof(struct uop_action_t));
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


  memdep = memdep_create(core, knobs->exec.memdep_opt_str);

  core->memory.mem_repeater = repeater_create(core->knobs->exec.repeater_opt_str, core, "MR1", core->memory.DL1);

  check_for_work = true;
}

void
core_exec_DPM_t::reg_stats(struct stat_sdb_t * const sdb)
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
  sprintf(buf,"c%d.load_nukes", arch->id);
  stat_reg_counter(sdb, true, buf, "num pipeflushes due to load-store order violation", &core->stat.load_nukes, 0, TRUE, NULL);
  sprintf(buf,"c%d.wp_load_nukes",arch->id);
  stat_reg_counter(sdb, true, buf, "num pipeflushes due to load-store order violation on wrong-path", &core->stat.wp_load_nukes, 0, TRUE, NULL);
  sprintf(buf,"c%d.DL1_load_split_accesses",arch->id);
  stat_reg_counter(sdb, true, buf, "number of loads requiring split accesses", &core->stat.DL1_load_split_accesses, 0, TRUE, NULL);
  sprintf(buf,"c%d.DL1_load_split_frac",arch->id);
  sprintf(buf2,"c%d.DL1_load_split_accesses/(c%d.DL1.load_lookups-c%d.DL1_load_split_accesses)",arch->id,arch->id,arch->id); /* need to subtract since each split access generated two load accesses */
  stat_reg_formula(sdb, true, buf, "fraction of loads requiring split accesses", buf2, NULL);

  sprintf(buf,"c%d.RS_occupancy",arch->id);
  stat_reg_counter(sdb, false, buf, "total RS occupancy", &core->stat.RS_occupancy, 0, TRUE, NULL);
  sprintf(buf,"c%d.RS_eff_occupancy",arch->id);
  stat_reg_counter(sdb, false, buf, "total RS effective occupancy", &core->stat.RS_eff_occupancy, 0, TRUE, NULL);
  sprintf(buf,"c%d.RS_empty",arch->id);
  stat_reg_counter(sdb, false, buf, "total cycles RS was empty", &core->stat.RS_empty_cycles, 0, TRUE, NULL);
  sprintf(buf,"c%d.RS_full",arch->id);
  stat_reg_counter(sdb, false, buf, "total cycles RS was full", &core->stat.RS_full_cycles, 0, TRUE, NULL);
  sprintf(buf,"c%d.RS_avg",arch->id);
  sprintf(buf2,"c%d.RS_occupancy/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "average RS occupancy", buf2, NULL);
  sprintf(buf,"c%d.RS_eff_avg",arch->id);
  sprintf(buf2,"c%d.RS_eff_occupancy/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "effective average RS occupancy", buf2, NULL);
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

  sprintf(buf,"c%d.int_FU_occupancy",arch->id);
  stat_reg_counter(sdb, true, buf, "int FUs occupancy", &core->stat.int_FU_occupancy, 0, TRUE, NULL);
  sprintf(buf,"c%d.fp_FU_occupancy",arch->id);
  stat_reg_counter(sdb, true, buf, "fp FUs occupancy", &core->stat.fp_FU_occupancy, 0, TRUE, NULL);
  sprintf(buf,"c%d.mul_FU_occupancy",arch->id);
  stat_reg_counter(sdb, true, buf, "mul FUs occupancy", &core->stat.mul_FU_occupancy, 0, TRUE, NULL);

  memdep->reg_stats(sdb, core);

  stat_reg_note(sdb,"\n#### DATA CACHE STATS ####");
  cache_reg_stats(sdb, core, core->memory.DL1);
  cache_reg_stats(sdb, core, core->memory.DTLB);
  if(core->memory.DTLB2)
    cache_reg_stats(sdb, core, core->memory.DTLB2);
  if(core->memory.DL2)
    cache_reg_stats(sdb, core, core->memory.DL2);
}

void core_exec_DPM_t::freeze_stats(void)
{
  memdep->freeze_stats();
}

void core_exec_DPM_t::update_occupancy(void)
{
    /* RS */
  core->stat.RS_occupancy += RS_num;
  core->stat.RS_eff_occupancy += RS_eff_num;
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

  for(int i=0; i<core->knobs->exec.num_exec_ports; i++)
  {
    for(int j=0; j<NUM_FU_CLASSES; j++)
      if(port[i].FU[j])
      {
        switch(j)
        {
          case FU_IEU:
          case FU_JEU:
          case FU_SHIFT:
            core->stat.int_FU_occupancy += port[i].FU[j]->occupancy;
            break;
          case FU_FADD:
          case FU_FMUL:
          case FU_FDIV:
          case FU_FCPLX:
            core->stat.fp_FU_occupancy += port[i].FU[j]->occupancy;
            break;
          case FU_IMUL:
          case FU_IDIV:
            core->stat.mul_FU_occupancy += port[i].FU[j]->occupancy;
            break;
          default:
            break;
        }
      }
  }
}

void core_exec_DPM_t::reset_execution(void)
{
  struct core_knobs_t * knobs = core->knobs;
  for(int i=0; i<knobs->exec.num_exec_ports; i++)
  {
    port[i].when_bypass_used = 0;

    for(int j=0; j<NUM_FU_CLASSES; j++)
      if(port[i].FU[j])
      {
        port[i].FU[j]->when_scheduleable = core->sim_cycle;
        port[i].FU[j]->when_executable = core->sim_cycle;
      }
  }
  check_for_work = true;
}

/* Functions to support dependency tracking 
   NOTE: "Ready" Queue is somewhat of a misnomer... uops are placed in the
   readyQ when all of their data-flow parents have issued (although not
   necessarily executed).  However, that doesn't necessarily mean that the
   corresponding input *values* are "ready" due to non-zero schedule-to-
   execute latencies. */
core_exec_DPM_t::readyQ_node_t * core_exec_DPM_t::get_readyQ_node(void)
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

void core_exec_DPM_t::return_readyQ_node(struct readyQ_node_t * const p)
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
void core_exec_DPM_t::insert_ready_uop(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  zesto_assert((uop->alloc.port_assignment >= 0) && (uop->alloc.port_assignment < knobs->exec.num_exec_ports),(void)0);
  zesto_assert(uop->timing.when_completed == TICK_T_MAX,(void)0);
  zesto_assert(uop->timing.when_exec == TICK_T_MAX,(void)0);
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

void core_exec_DPM_t::RS_schedule(void) /* for uops in the RS */
{
  struct core_knobs_t * knobs = core->knobs;
  int i;

  /* select/pick from ready instructions and send to exec ports */
  for(i=0;i<knobs->exec.num_exec_ports;i++)
  {
    struct readyQ_node_t * rq = port[i].readyQ;
    struct readyQ_node_t * prev = NULL;
    int issued = false;

    if(port[i].payload_pipe[0].uop == NULL) /* port is free */
      while(rq) /* if anyone's waiting to issue to this port */
      {
        struct uop_t * uop = rq->uop;

#ifdef ZTRACE
        if(uop->timing.when_ready == core->sim_cycle)
          ztrace_print(uop,"e|ready|uop ready for scheduling");
#endif

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
                (port[i].FU[uop->decode.FU_class]->when_scheduleable <= core->sim_cycle) &&
                ((!uop->decode.in_fusion) || uop->decode.fusion_head->alloc.full_fusion_allocated))
        {
          zesto_assert(uop->alloc.port_assignment == i,(void)0);

          port[i].payload_pipe[0].uop = uop;
          port[i].payload_pipe[0].action_id = uop->exec.action_id;
          port[i].occupancy++;
          zesto_assert(port[i].occupancy <= knobs->exec.payload_depth,(void)0);
          uop->timing.when_issued = core->sim_cycle;
          check_for_work = true;

#ifdef ZTRACE
          ztrace_print(uop,"e|RS-issue|uop issued to payload RAM");
#endif

          if(uop->decode.is_load)
          {
            int fp_penalty = REG_IS_FPR(uop->decode.odep_name)?knobs->exec.fp_penalty:0;
            uop->timing.when_otag_ready = core->sim_cycle + port[i].FU[uop->decode.FU_class]->latency + core->memory.DL1->latency + fp_penalty;
          }
          else
          {
            int fp_penalty = ((REG_IS_FPR(uop->decode.odep_name) && !(uop->decode.opflags & F_FCOMP)) ||
                             (!REG_IS_FPR(uop->decode.odep_name) && (uop->decode.opflags & F_FCOMP)))?knobs->exec.fp_penalty:0;
            uop->timing.when_otag_ready = core->sim_cycle + port[i].FU[uop->decode.FU_class]->latency + fp_penalty;
          }

          port[i].FU[uop->decode.FU_class]->when_scheduleable = core->sim_cycle + port[i].FU[uop->decode.FU_class]->issue_rate;

          /* tag broadcast to dependents */
          struct odep_t * odep = uop->exec.odep_uop;
          while(odep)
          {
            int j;
            tick_t when_ready = 0;
            odep->uop->timing.when_itag_ready[odep->op_num] = uop->timing.when_otag_ready;
            for(j=0;j<MAX_IDEPS;j++)
            {
              if(when_ready < odep->uop->timing.when_itag_ready[j])
                when_ready = odep->uop->timing.when_itag_ready[j];
            }
            odep->uop->timing.when_ready = when_ready;

            if(when_ready < TICK_T_MAX)
              insert_ready_uop(odep->uop);

            odep = odep->next;
          }

          if(uop->decode.is_load)
          {
            zesto_assert((uop->alloc.LDQ_index >= 0) && (uop->alloc.LDQ_index < knobs->exec.LDQ_size),(void)0);
            LDQ[uop->alloc.LDQ_index].speculative_broadcast = true;
          }

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
bool core_exec_DPM_t::check_load_issue_conditions(const struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  /* are all previous STA's known? If there's a match, is the STD ready? */
  bool sta_unknown = false;
  bool regular_match = false;
  bool partial_match = false;
  bool oracle_regular_match = false;
  bool oracle_partial_match = false;
  int i;
  int match_index = -1;
  int oracle_index = -1;
  int num_stores = 0; /* need this extra condition because STQ could be full with all stores older than the load we're considering */

  /* don't reissue someone who's already issued */
  zesto_assert((uop->alloc.LDQ_index >= 0) && (uop->alloc.LDQ_index < knobs->exec.LDQ_size),false);
  if(LDQ[uop->alloc.LDQ_index].when_issued != TICK_T_MAX)
    return false;

  /* Conservative fence implementation -- if there is an older fence in LDQ,
   * don't issue. */
  for (int j = LDQ_head; j != uop->alloc.LDQ_index; j = modinc(j, knobs->exec.LDQ_size)) {
    if (LDQ[j].uop->decode.is_fence &&
        LDQ[j].uop->timing.when_completed == TICK_T_MAX)
      return false;
  }

  md_addr_t ld_addr1 = LDQ[uop->alloc.LDQ_index].uop->oracle.virt_addr;
  md_addr_t ld_addr2 = LDQ[uop->alloc.LDQ_index].virt_addr + uop->decode.mem_size - 1;

  /* this searches the senior STQ as well. */
  for(i=LDQ[uop->alloc.LDQ_index].store_color;
      ((modinc(i,knobs->exec.STQ_size)) != STQ_senior_head) && (STQ[i].uop_seq < uop->decode.uop_seq) && (num_stores < STQ_senior_num);
      i=moddec(i,knobs->exec.STQ_size) )
  {
    /* check addr match */
    int st_mem_size = STQ[i].mem_size;
    int ld_mem_size = uop->decode.mem_size;
    md_addr_t st_addr1, st_addr2;
    
    if(STQ[i].addr_valid)
      st_addr1 = STQ[i].virt_addr; /* addr of first byte */
    else
    {
      zesto_assert(STQ[i].sta,false);
      st_addr1 = STQ[i].sta->oracle.virt_addr; /* addr of first byte */
      sta_unknown = true;
    }
    st_addr2 = st_addr1 + st_mem_size - 1; /* addr of last byte */

    zesto_assert(st_mem_size,false);
    zesto_assert(ld_mem_size,false);
    /* XXX: The following assert should be used, but under RHEL5-compiled binaries,
       there seems to be some weird thing going on where the GS segment register is
       getting zeroed out somewhere which later leads to a null-pointer dereference.
       On some test programs, this didn't seem to cause any problems, so for now
       we're just leaving it this way. */
    //zesto_assert(uop->Mop->oracle.spec_mode || (ld_addr1 && ld_addr2),false);
    zesto_assert((st_addr1 && st_addr2) || (STQ[i].sta && STQ[i].sta->Mop->oracle.spec_mode),false);

    if((st_addr1 <= ld_addr1) && (st_addr2 >= ld_addr2)) /* store write fully overlaps the load address range */
    {
      if((match_index == -1) && (STQ[i].addr_valid))
      {
        match_index = i;
        regular_match = true;
      }

      if(oracle_index == -1)
      {
        oracle_index = i;
        oracle_regular_match = true;
      }
    }
    else if((st_addr2 < ld_addr1) || (st_addr1 > ld_addr2)) /* store addr is completely before or after the load */
    {
      /* no conflict here */
    }
    else /* partial store */
    {
      if((match_index == -1) && (STQ[i].addr_valid))
      {
        match_index = i;
        partial_match = true;
      }

      if(oracle_index == -1)
      {
        oracle_index = i;
        oracle_partial_match = true;
      }
    }

    num_stores++;
  }

  if(partial_match)
  {
    /* in presence of (known) partial store forwarding, stall until store *commits* */
    LDQ[uop->alloc.LDQ_index].partial_forward = true;
    return false;
  }

  return memdep->lookup(uop->Mop->fetch.PC,sta_unknown,oracle_regular_match,oracle_partial_match);
}

/* helper function to remove issued uops after a latency misprediction */
void core_exec_DPM_t::snatch_back(struct uop_t * const replayed_uop)
{
  struct core_knobs_t * knobs = core->knobs;
  int i;
  int index = 0;
  struct uop_t ** stack = (struct uop_t**) alloca(sizeof(*stack) * knobs->exec.RS_size);
  if(!stack)
    fatal("couldn't alloca snatch_back_stack");
  stack[0] = replayed_uop;

  while(index >= 0)
  {
    struct uop_t * uop = stack[index];

    /* reset current uop */
    uop->timing.when_issued = TICK_T_MAX;
    uop->timing.when_exec = TICK_T_MAX;
    uop->timing.when_completed = TICK_T_MAX;
    uop->timing.when_otag_ready = TICK_T_MAX;
    if(uop->decode.is_load)
    {
      zesto_assert((uop->alloc.LDQ_index >= 0) && (uop->alloc.LDQ_index < knobs->exec.LDQ_size),(void)0);
      zesto_assert(uop->timing.when_completed == TICK_T_MAX,(void)0);
      LDQ[uop->alloc.LDQ_index].hit_in_STQ = false;
      LDQ[uop->alloc.LDQ_index].addr_valid = false;
      LDQ[uop->alloc.LDQ_index].when_issued = TICK_T_MAX;
      LDQ[uop->alloc.LDQ_index].first_byte_requested = false;
      LDQ[uop->alloc.LDQ_index].last_byte_requested = false;
      LDQ[uop->alloc.LDQ_index].first_byte_arrived = false;
      LDQ[uop->alloc.LDQ_index].last_byte_arrived = false;
    }

    /* remove uop from payload RAM pipe */
    if(port[uop->alloc.port_assignment].occupancy > 0)
      for(i=0;i<knobs->exec.payload_depth;i++)
        if(port[uop->alloc.port_assignment].payload_pipe[i].uop == uop)
        {
          port[uop->alloc.port_assignment].payload_pipe[i].uop = NULL;
          port[uop->alloc.port_assignment].occupancy--;
          zesto_assert(port[uop->alloc.port_assignment].occupancy >= 0,(void)0);
          ZESTO_STAT(core->stat.exec_uops_snatched_back++;)
          break;
        }

    /* remove uop from readyQ */
    uop->exec.action_id = core->new_action_id();
    uop->exec.in_readyQ = false;

    /* remove uop from squash list */
    stack[index] = NULL;
    index --;

    /* add dependents to squash list */
    struct odep_t * odep = uop->exec.odep_uop;
    while(odep)
    {
      /* squash this input tag */
      odep->uop->timing.when_itag_ready[odep->op_num] = TICK_T_MAX;

      if(odep->uop->timing.when_issued != TICK_T_MAX) /* if the child issued */
      {
        index ++;
        stack[index] = odep->uop;
      }
      if(odep->uop->exec.in_readyQ) /* if child is in the readyQ */
      {
        odep->uop->exec.action_id = core->new_action_id();
        odep->uop->exec.in_readyQ = false;
      }
      odep = odep->next;
    }
  }

  tick_t when_ready = 0;
  for(i=0;i<MAX_IDEPS;i++)
    if(replayed_uop->timing.when_itag_ready[i] > when_ready)
      when_ready = replayed_uop->timing.when_itag_ready[i];
  /* we assume if a when_ready has been reset to TICK_T_MAX, then
     the parent will re-wakeup the child.  In any case, we also
     assume that a parent that takes longer to execute than
     originally predicted will update its children's
     when_itag_ready's to the new value */
  if(when_ready != TICK_T_MAX) /* otherwise reschedule the uop */
  {
    replayed_uop->timing.when_ready = when_ready;
    insert_ready_uop(replayed_uop);
  }
}

/* The callback functions below (after load_writeback) mark flags
   in the uop to specify the completion of each task, and only when
   all are done do we call the load-writeback function to finish
   off execution of the load. */

void core_exec_DPM_t::load_writeback(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  zesto_assert((uop->alloc.LDQ_index >= 0) && (uop->alloc.LDQ_index < knobs->exec.LDQ_size),(void)0);
  if(!LDQ[uop->alloc.LDQ_index].hit_in_STQ) /* no match in STQ, so use cache value */
  {
#ifdef ZTRACE
    ztrace_print(uop,"e|load|writeback from cache/writeback");
#endif

    int fp_penalty = REG_IS_FPR(uop->decode.odep_name)?knobs->exec.fp_penalty:0;

    port[uop->alloc.port_assignment].when_bypass_used = core->sim_cycle+fp_penalty;
    uop->exec.ovalue = uop->oracle.ovalue; /* XXX: just using oracle value for now */
    uop->exec.ovalue_valid = true;
    zesto_assert(uop->timing.when_completed == TICK_T_MAX,(void)0);
    uop->timing.when_completed = core->sim_cycle+fp_penalty;
    update_last_completed(core->sim_cycle+fp_penalty); /* for deadlock detection */
    if(uop->decode.is_ctrl && (uop->Mop->oracle.NextPC != uop->Mop->fetch.pred_NPC)) /* XXX: for RETN */
    {
      core->oracle->pipe_recover(uop->Mop,uop->Mop->oracle.NextPC);
      ZESTO_STAT(core->stat.num_jeclear++;)
      if(uop->Mop->oracle.spec_mode)
        ZESTO_STAT(core->stat.num_wp_jeclear++;)
#ifdef ZTRACE
      ztrace_print(uop,"e|jeclear|load/RETN mispred detected (no STQ hit)");
#endif
    }


    if(uop->timing.when_otag_ready > (core->sim_cycle+fp_penalty))
      /* we thought this output would be ready later in the future,
         but it's ready now! */
      uop->timing.when_otag_ready = core->sim_cycle+fp_penalty;

    /* bypass output value to dependents, but also check to see if
       dependents were already speculatively scheduled; if not,
       wake them up. */
    struct odep_t * odep = uop->exec.odep_uop;

    while(odep)
    {
      /* check scheduling info (tags) */
      if(odep->uop->timing.when_itag_ready[odep->op_num] > (core->sim_cycle+fp_penalty))
      {
        int j;
        tick_t when_ready = 0;

        odep->uop->timing.when_itag_ready[odep->op_num] = core->sim_cycle+fp_penalty;

        for(j=0;j<MAX_IDEPS;j++)
          if(when_ready < odep->uop->timing.when_itag_ready[j])
            when_ready = odep->uop->timing.when_itag_ready[j];

        if(when_ready < TICK_T_MAX)
        {
          odep->uop->timing.when_ready = when_ready;
          if(!odep->uop->exec.in_readyQ)
            insert_ready_uop(odep->uop);
        }
      }

      /* bypass value */
      zesto_assert(!odep->uop->exec.ivalue_valid[odep->op_num],(void)0);
      odep->uop->exec.ivalue_valid[odep->op_num] = true;
      if(odep->aflags) /* shouldn't happen for loads? */
      {
        warn("load modified flags?\n");
        odep->uop->exec.ivalue[odep->op_num].dw = uop->exec.oflags;
      }
      else
        odep->uop->exec.ivalue[odep->op_num] = uop->exec.ovalue;
      odep->uop->timing.when_ival_ready[odep->op_num] = core->sim_cycle+fp_penalty;

      odep = odep->next;
    }

  }
}

/* used for accesses that can be serviced entirely by one cacheline,
   or by the first access of a split-access */
void core_exec_DPM_t::DL1_callback(void * const op)
{
  struct uop_t * uop = (struct uop_t*) op;
  class core_exec_DPM_t * E = (core_exec_DPM_t*)uop->core->exec;
  if(uop->alloc.LDQ_index != -1)
  {
    struct LDQ_t * LDQ_item = &E->LDQ[uop->alloc.LDQ_index];
#ifdef ZTRACE
    ztrace_print(uop,"e|load|returned from cache/memory");
#endif

    LDQ_item->first_byte_arrived = true;

    /* Access was a hit in repeater */
    if(LDQ_item->first_repeated && LDQ_item->repeater_first_arrived)
      return;

    /* We don't care about the repeater (in general, or it has arrived with a  miss),
     * check for split load and TLB access */
    if((uop->exec.when_addr_translated <= uop->core->sim_cycle) &&
        LDQ_item->all_arrived() &&
        !LDQ_item->hit_in_STQ) /* no match in STQ, so use cache value */
    {
      /* if load received value from STQ, it could have already
         committed by the time this gets called (esp. if we went to
         main memory) */
      uop->exec.when_data_loaded = uop->core->sim_cycle;
      E->load_writeback(uop);
    }
  }
}

/* used only for the second access of a split access */
void core_exec_DPM_t::DL1_split_callback(void * const op)
{
  struct uop_t * uop = (struct uop_t*) op;
  class core_exec_DPM_t * E = (core_exec_DPM_t*)uop->core->exec;
  if(uop->alloc.LDQ_index != -1)
  {
    struct LDQ_t * LDQ_item = &E->LDQ[uop->alloc.LDQ_index];
#ifdef ZTRACE
    ztrace_print(uop,"e|load|split returned from cache/memory");
#endif

    LDQ_item->last_byte_arrived = true;

    /* Access was a hit in repeater */
    if(LDQ_item->last_repeated && LDQ_item->repeater_last_arrived)
      return;

    /* We don't care about the repeater (in general, or it has arrived with a miss),
     * check for split load and TLB access */
    if((uop->exec.when_addr_translated <= uop->core->sim_cycle) &&
        LDQ_item->all_arrived() &&
        !LDQ_item->hit_in_STQ) /* no match in STQ, so use cache value */
    {

      /* if load received value from STQ, it could have already
         committed by the time this gets called (esp. if we went to
         main memory) */
      uop->exec.when_data_loaded = uop->core->sim_cycle;
      E->load_writeback(uop);
    }
  }
}

void core_exec_DPM_t::repeater_callback(void * const op, bool is_hit)
{
  struct uop_t * uop = (struct uop_t*) op;
  struct core_t * core = uop->core;
  struct core_knobs_t * knobs = core->knobs;
  class core_exec_DPM_t * E = (core_exec_DPM_t*)uop->core->exec;

  if(uop->alloc.LDQ_index != -1)
  {
    struct LDQ_t * LDQ_item = &E->LDQ[uop->alloc.LDQ_index];
#ifdef ZTRACE
    ztrace_print(uop,"e|load|returned from repeater (hit: %d) %d %d", is_hit, uop->exec.when_addr_translated, E->LDQ[uop->alloc.LDQ_index].repeater_last_arrived);
#endif
    zesto_assert(LDQ_item->first_repeated, (void)0);
    LDQ_item->repeater_first_arrived = true;

    /* Repeater doesn't have this address */
    if (!is_hit) {
      /* We don't have a parallel DL1 request */
      if(!knobs->memory.DL1_rep_req) {
        /* Schedule it to DL1 */
        LDQ_item->uop->oracle.is_repeated = false;
        LDQ_item->when_issued = TICK_T_MAX;
        LDQ_item->first_byte_requested = false;
        LDQ_item->first_byte_arrived = false;
        LDQ_item->repeater_first_arrived = false;
      }
      /* We have a parallel DL1 request */
      else {
        /* If we have the value from DL1 already, mark as done */
        if((uop->exec.when_addr_translated <= core->sim_cycle) &&
            LDQ_item->all_arrived() &&
            !E->LDQ[uop->alloc.LDQ_index].hit_in_STQ) /* no match in STQ, so use cache value */
        {
          /* if load received value from STQ, it could have already
           * committed by the time this gets called (esp. if we went to
           * main memory) */
          uop->exec.when_data_loaded = core->sim_cycle;
          E->load_writeback(uop);
        }
        else {
          /* if this is not a split access, but we still wait on
           * something else, make sure DL1/DTLB handlers know that
           * we've missed for sure in the repeater */
          if(LDQ_item->repeater_last_arrived)
            LDQ_item->first_repeated = false;
        }

        /* - split access, TBD in split handler */
      }
    } else {
      /* Ok, repeater has value, check split access */
      if ((uop->exec.when_addr_translated <= core->sim_cycle) &&
          LDQ_item->all_arrived() &&
          !E->LDQ[uop->alloc.LDQ_index].hit_in_STQ) /* no match in STQ, so use cache value */
        {
          uop->exec.when_data_loaded = core->sim_cycle;
          E->load_writeback(uop);
        }
    }
  }
}

/* used only for the second access of a split repeater access */
void core_exec_DPM_t::repeater_split_callback(void * const op, bool is_hit)
{
  struct uop_t * uop = (struct uop_t*) op;
  struct core_t * core = uop->core;
  struct core_knobs_t * knobs = core->knobs;
  class core_exec_DPM_t * E = (core_exec_DPM_t*)uop->core->exec;

  if(uop->alloc.LDQ_index != -1)
  {
    struct LDQ_t * LDQ_item = &E->LDQ[uop->alloc.LDQ_index];
#ifdef ZTRACE
    ztrace_print(uop,"e|load|split returned from repeater (hit:%d)", is_hit);
#endif
    zesto_assert(LDQ_item->last_repeated, (void)0);

    LDQ_item->repeater_last_arrived = true;

    /* Repeater hit, now check if first access and DTLB have arrived */
    if (is_hit)
    {
      if((uop->exec.when_addr_translated <= core->sim_cycle) &&
         LDQ_item->all_arrived() &&
         !LDQ_item->hit_in_STQ) /* no match in STQ, so use cache value */
      {
        /* if load received value from STQ, it could have already
           committed by the time this gets called (esp. if we went to
           main memory) */
        uop->exec.when_data_loaded = core->sim_cycle;
        E->load_writeback(uop);
      }
    }
    /* Repeater miss, need to check if we are done at DL1/DTLB */
    else {
      /* We don't have a parallel DL1 request */
      if(!knobs->memory.DL1_rep_req) {
        /* Schedule it to DL1 */
        LDQ_item->uop->oracle.is_repeated = false;
        LDQ_item->when_issued = TICK_T_MAX;
        LDQ_item->last_byte_requested = false;
        LDQ_item->last_byte_arrived = false;
        LDQ_item->repeater_last_arrived = false;
        /* we still wait on something else, make sure other
         * handlers know that we've missed for sure in the repeater */
      }
      else {
        /* We have a parallel DL1 request */
        if((uop->exec.when_addr_translated <= core->sim_cycle) &&
            LDQ_item->all_arrived() &&
            !E->LDQ[uop->alloc.LDQ_index].hit_in_STQ) /* no match in STQ, so use cache value */
        {
          /* if load received value from STQ, it could have already
             committed by the time this gets called (esp. if we went to
             main memory) */
          uop->exec.when_data_loaded = core->sim_cycle;
          E->load_writeback(uop);
        }
        else {
          /* we still wait on something else, make sure other
           * handlers know that we've missed for sure in the repeater */
           LDQ_item->last_repeated = false;
        }
      }
    }
  }
}
void core_exec_DPM_t::DTLB_callback(void * const op)
{
  struct uop_t * uop = (struct uop_t*) op;
  struct core_t * core = uop->core;
  struct core_exec_DPM_t * E = (core_exec_DPM_t*)uop->core->exec;
  if(uop->alloc.LDQ_index != -1)
  {
    struct LDQ_t * LDQ_item = &E->LDQ[uop->alloc.LDQ_index];
    zesto_assert(uop->exec.when_addr_translated == TICK_T_MAX,(void)0);
    uop->exec.when_addr_translated = core->sim_cycle;
#ifdef ZTRACE
    ztrace_print(uop,"e|load|virtual address translated");
#endif
    if((uop->exec.when_addr_translated <= core->sim_cycle) &&
        LDQ_item->all_arrived() &&
        !E->LDQ[uop->alloc.LDQ_index].hit_in_STQ)
      E->load_writeback(uop);
  }
}

/* returns true if TLB translation has completed */
bool core_exec_DPM_t::translated_callback(void * const op, const seq_t action_id)
{
  struct uop_t * uop = (struct uop_t*) op;
  if((uop->exec.action_id == action_id) && (uop->alloc.LDQ_index != -1))
    return uop->exec.when_addr_translated <= uop->core->sim_cycle;
  else
    return true;
}

/* Used by the cache processing functions to recover the id of the
   uop without needing to know about the uop struct. */
seq_t core_exec_DPM_t::get_uop_action_id(void * const op)
{
  struct uop_t const * uop = (struct uop_t*) op;
  return uop->exec.action_id;
}

/* This function takes care of uops that have been misscheduled due
   to a latency misprediction (e.g., they got scheduled assuming
   the parent uop would hit in the DL1, but it turns out that the
   load latency will be longer due to a cache miss).  Uops may be
   speculatively rescheduled based on the next anticipated latency
   (e.g., L2 hit latency). */
void core_exec_DPM_t::load_miss_reschedule(void * const op, const int new_pred_latency)
{
  struct uop_t * uop = (struct uop_t*) op;
  struct core_t * core = uop->core;
  struct core_exec_DPM_t * E = (core_exec_DPM_t*)core->exec;
  struct core_knobs_t * knobs = core->knobs;
  zesto_assert(uop->decode.is_load,(void)0);

  /* Hit in repeater was already processed */
  if (uop->alloc.LDQ_index == -1 || !uop->decode.is_load    // uop was alredy recycled
      || uop->timing.when_completed != TICK_T_MAX)          // or not, but is already marked as completed
    return;

  /* if we've speculatively woken up our dependents, we need to
     snatch them back out of the execution pipeline and replay them
     later (but if the load already hit in the STQ, then don't need
     to replay) */
  if(E->LDQ[uop->alloc.LDQ_index].speculative_broadcast && !E->LDQ[uop->alloc.LDQ_index].hit_in_STQ)
  {
#ifdef ZTRACE
    ztrace_print(uop,"e|cache-miss|no STQ hit");
#endif
    /* we speculatively scheduled children assuming we'd hit in the
       previous cache level, but we didn't... so put children back
       to sleep */
    uop->timing.when_otag_ready = TICK_T_MAX;
    struct odep_t * odep = uop->exec.odep_uop;
    while(odep)
    {
      odep->uop->timing.when_itag_ready[odep->op_num] = TICK_T_MAX;
      E->snatch_back(odep->uop);

      odep = odep->next;
    }

    /* now assume a hit in this cache level */
    odep = uop->exec.odep_uop;
    if(new_pred_latency != BIG_LATENCY)
      uop->timing.when_otag_ready = core->sim_cycle + new_pred_latency - knobs->exec.payload_depth - 1;

    while(odep)
    {
      odep->uop->timing.when_itag_ready[odep->op_num] = uop->timing.when_otag_ready;

      /* put back on to readyQ if appropriate */
      int j;
      tick_t when_ready = 0;
      zesto_assert(!odep->uop->exec.in_readyQ,(void)0);

      for(j=0;j<MAX_IDEPS;j++)
        if(when_ready < odep->uop->timing.when_itag_ready[j])
          when_ready = odep->uop->timing.when_itag_ready[j];

      odep->uop->timing.when_ready = when_ready;
      if(when_ready < TICK_T_MAX)
      {
        E->insert_ready_uop(odep->uop);
      }

      odep = odep->next;
    }
  }
#ifdef ZTRACE
  else
    ztrace_print(uop,"e|cache-miss|STQ hit so miss not observed");
#endif

}

/* process loads exiting the STQ search pipeline, update caches */
void core_exec_DPM_t::LDST_exec(void)
{
  struct core_knobs_t * knobs = core->knobs;
  int i;

  /* shuffle STQ pipeline forward */
  for(i=0;i<knobs->exec.port_binding[FU_LD].num_FUs;i++)
  {
    int port_num = knobs->exec.port_binding[FU_LD].ports[i];
    int stage = port[port_num].STQ->latency-1;
    int j;
    for(j=stage;j>0;j--)
      port[port_num].STQ->pipe[j] = port[port_num].STQ->pipe[j-1];
    port[port_num].STQ->pipe[0].uop = NULL;
  }

  /* process STQ pipes */
  for(i=0;i<knobs->exec.port_binding[FU_LD].num_FUs;i++)
  {
    int port_num = knobs->exec.port_binding[FU_LD].ports[i];
    int stage = port[port_num].STQ->latency-1;
    int j;

    struct uop_t * uop = port[port_num].STQ->pipe[stage].uop;


    if(uop && (port[port_num].STQ->pipe[stage].action_id == uop->exec.action_id))
    {
#ifdef ZTRACE
      ztrace_print(uop,"e|STQ|load searches STQ for addr match");
#endif
      int num_stores = 0;
      zesto_assert((uop->alloc.LDQ_index >= 0) && (uop->alloc.LDQ_index < knobs->exec.LDQ_size),(void)0);

      /* check STQ for match, including senior STQ */
      /*for(j=LDQ[uop->alloc.LDQ_index].store_color;
          STQ[j].sta && (STQ[j].sta->decode.uop_seq < uop->decode.uop_seq) && (num_stores < STQ_senior_num);
          j=(j-1+knobs->exec.STQ_size) % knobs->exec.STQ_size)*/

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
      int cond3 = (num_stores < STQ_senior_num);

      while(cond1 && cond2 && cond3)
      {
        int st_mem_size = STQ[j].mem_size;
        int ld_mem_size = uop->decode.mem_size;
        md_addr_t st_addr1 = STQ[j].virt_addr; /* addr of first byte */
        md_addr_t st_addr2 = STQ[j].virt_addr + st_mem_size - 1; /* addr of last byte */
        md_addr_t ld_addr1 = LDQ[uop->alloc.LDQ_index].virt_addr;
        md_addr_t ld_addr2 = LDQ[uop->alloc.LDQ_index].virt_addr + ld_mem_size - 1;

        if(STQ[j].addr_valid)
        {
          if((st_addr1 <= ld_addr1) && (st_addr2 >= ld_addr2)) /* match */
          {
            if(uop->timing.when_completed != TICK_T_MAX)
            {
              memdep->update(uop->Mop->fetch.PC);
              core->oracle->pipe_flush(uop->Mop);
              ZESTO_STAT(core->stat.load_nukes++;)
              if(uop->Mop->oracle.spec_mode)
                ZESTO_STAT(core->stat.wp_load_nukes++;)
#ifdef ZTRACE
              ztrace_print(uop,"e|order-violation|matching store found but load already executed");
#endif
            }
            else
            {
              if(STQ[j].value_valid)
              {
                int fp_penalty = REG_IS_FPR(uop->decode.odep_name)?knobs->exec.fp_penalty:0;
                uop->exec.ovalue = STQ[j].value;
                uop->exec.ovalue_valid = true;
                uop->exec.action_id = core->new_action_id();
                zesto_assert(uop->timing.when_completed == TICK_T_MAX,(void)0);
                uop->timing.when_completed = core->sim_cycle+fp_penalty;
                uop->timing.when_exec = core->sim_cycle+fp_penalty;
                update_last_completed(core->sim_cycle+fp_penalty); /* for deadlock detection */

                /* when_issued should be != TICK_T_MAX; in a few cases I was
                   finding it set; haven't had time to fully debug this yet,
                   but this fix apparently works for now. */
                if(LDQ[uop->alloc.LDQ_index].when_issued == TICK_T_MAX)
                  LDQ[uop->alloc.LDQ_index].when_issued = core->sim_cycle+fp_penalty;
                LDQ[uop->alloc.LDQ_index].hit_in_STQ = true;
#ifdef ZTRACE
                ztrace_print(uop,"e|STQ-hit|store-to-load forward");
#endif

                if(uop->decode.is_ctrl && (uop->Mop->oracle.NextPC != uop->Mop->fetch.pred_NPC)) /* for RETN */
                {
                  core->oracle->pipe_recover(uop->Mop,uop->Mop->oracle.NextPC);
                  ZESTO_STAT(core->stat.num_jeclear++;)
                  if(uop->Mop->oracle.spec_mode)
                    ZESTO_STAT(core->stat.num_wp_jeclear++;)
#ifdef ZTRACE
                  ztrace_print(uop,"e|jeclear|load/RETN mispred detected in STQ hit");
#endif
                }

                /* we thought this output would be ready later in the future, but
                   it's ready now! */
                if(uop->timing.when_otag_ready > (core->sim_cycle+fp_penalty))
                  uop->timing.when_otag_ready = core->sim_cycle+fp_penalty;

                struct odep_t * odep = uop->exec.odep_uop;

                while(odep)
                {
                  /* check scheduling info (tags) */
                  if(odep->uop->timing.when_itag_ready[odep->op_num] > core->sim_cycle)
                  {
                    int j;
                    tick_t when_ready = 0;

                    odep->uop->timing.when_itag_ready[odep->op_num] = core->sim_cycle+fp_penalty;

                    for(j=0;j<MAX_IDEPS;j++)
                      if(when_ready < odep->uop->timing.when_itag_ready[j])
                        when_ready = odep->uop->timing.when_itag_ready[j];

                    if(when_ready < TICK_T_MAX)
                    {
                      odep->uop->timing.when_ready = when_ready;
                      if(!odep->uop->exec.in_readyQ)
                        insert_ready_uop(odep->uop);
                    }
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
                  odep->uop->timing.when_ival_ready[odep->op_num] = core->sim_cycle+fp_penalty;

                  odep = odep->next;
                }
              }
              else /* addr match but STD unknown */
              {
#ifdef ZTRACE
                ztrace_print(uop,"e|STQ-miss|addr hit but data not ready");
#endif
                /* reset the load's children */
                if(LDQ[uop->alloc.LDQ_index].speculative_broadcast)
                {
                  uop->timing.when_otag_ready = TICK_T_MAX;
                  uop->timing.when_completed = TICK_T_MAX;
                  struct odep_t * odep = uop->exec.odep_uop;
                  while(odep)
                  {
                    odep->uop->timing.when_itag_ready[odep->op_num] = TICK_T_MAX;
                    odep->uop->exec.ivalue_valid[odep->op_num] = false;
#ifdef ZTRACE
                    if(odep->uop->timing.when_issued != TICK_T_MAX)
                      ztrace_print(odep->uop,"e|snatch-back|parent STQ data not ready");
#endif
                    snatch_back(odep->uop);

                    odep = odep->next;
                  }
                  /* clear flag so we don't keep doing this over and over again */
                  LDQ[uop->alloc.LDQ_index].speculative_broadcast = false;
                }
                uop->exec.action_id = core->new_action_id();
                LDQ[uop->alloc.LDQ_index].hit_in_STQ = false;
                LDQ[uop->alloc.LDQ_index].first_byte_requested = false;
                LDQ[uop->alloc.LDQ_index].last_byte_requested = false;
                LDQ[uop->alloc.LDQ_index].first_byte_arrived = false;
                LDQ[uop->alloc.LDQ_index].last_byte_arrived = false;
                LDQ[uop->alloc.LDQ_index].repeater_first_arrived = false;
                LDQ[uop->alloc.LDQ_index].repeater_last_arrived = false;
                /* When_issued is still set, so LDQ won't reschedule the load.  The load
                   will wait until the STD finishes and wakes it up. */
              }
            }
            break;
          }
          else if((st_addr2 < ld_addr1) || (st_addr1 > ld_addr2)) /* no overlap */
          {
            /* nothing to do */
          }
          else /* partial match */
          {
            LDQ[uop->alloc.LDQ_index].partial_forward = true;

            /* squash if load already executed */
            if(uop->timing.when_completed != TICK_T_MAX)
            {
              /* flush is slightly different from branch mispred -
                 on branch, we squash everything *after* the branch,
                 whereas with this flush (e.g., used for memory
                 order violations), we squash the Mop itself as well
                 as everything after it */
              memdep->update(uop->Mop->fetch.PC);
              core->oracle->pipe_flush(uop->Mop);
              ZESTO_STAT(core->stat.load_nukes++;)
              if(uop->Mop->oracle.spec_mode)
                ZESTO_STAT(core->stat.wp_load_nukes++;)
#ifdef ZTRACE
              ztrace_print(uop,"e|order-violation|partially matching store found but load already executed");
#endif
            }
            /* reset the load's children - the load's going to have to wait until this store commits */
            else if(LDQ[uop->alloc.LDQ_index].speculative_broadcast)
            {
              uop->timing.when_otag_ready = TICK_T_MAX;
              uop->timing.when_completed = TICK_T_MAX;
              struct odep_t * odep = uop->exec.odep_uop;
              while(odep)
              {
                odep->uop->timing.when_itag_ready[odep->op_num] = TICK_T_MAX;
                odep->uop->exec.ivalue_valid[odep->op_num] = false;
#ifdef ZTRACE
                if(odep->uop->timing.when_issued != TICK_T_MAX)
                  ztrace_print(odep->uop,"e|snatch-back|parent had partial match in STQ");
#endif
                snatch_back(odep->uop);
                odep = odep->next;
              }
              /* clear flag so we don't keep doing this over and over again */
              LDQ[uop->alloc.LDQ_index].speculative_broadcast = false;
            }
            uop->exec.action_id = core->new_action_id();

            /* different from STD-not-ready case above, in the case of a partial match,
               the load may not get woken up by the store (and even if it does, the
               load still can't execute, so we reset the load's when_issued so that
               LDQ_schedule keeps polling until the partial-matching store retires. */
            zesto_assert(uop->timing.when_completed == TICK_T_MAX,(void)0);
            LDQ[uop->alloc.LDQ_index].when_issued = TICK_T_MAX;
            LDQ[uop->alloc.LDQ_index].hit_in_STQ = false;
            LDQ[uop->alloc.LDQ_index].first_byte_requested = false;
            LDQ[uop->alloc.LDQ_index].last_byte_requested = false;
            LDQ[uop->alloc.LDQ_index].first_byte_arrived = false;
            LDQ[uop->alloc.LDQ_index].last_byte_arrived = false;
            LDQ[uop->alloc.LDQ_index].repeater_first_arrived = false;
            LDQ[uop->alloc.LDQ_index].repeater_last_arrived = false;

            /* On a case where partial store forwarding occurs, the load doesn't really
               "know" when the partially-matched store finishes writing back to the cache.
               So what we do is we stop the load from polling the STQ, except for immediately
               after each store (whether matching or not) writes back and deallocates from
               the senior store queue (see STQ_deallocate_senior). */
            if(knobs->exec.throttle_partial)
              partial_forward_throttle = true;
            break;
          }
        }


        num_stores++;

        j=moddec(j,knobs->exec.STQ_size); //(j-1+knobs->exec.STQ_size) % knobs->exec.STQ_size;

        cond1 = STQ[j].sta != NULL;
        cond2 = cond1 && (STQ[j].sta->decode.uop_seq < uop->decode.uop_seq);
        cond3 = (num_stores < STQ_senior_num);
      }
    }
  }

  lk_lock(&cache_lock, core->id+1);
  if(core->memory.DTLB2) cache_process(core->memory.DTLB2);
  cache_process(core->memory.DTLB);
  if(core->memory.DL2) cache_process(core->memory.DL2);
  cache_process(core->memory.DL1);
  lk_unlock(&cache_lock);
}

/* Schedule load uops to execute from the LDQ.  Load execution occurs in a two-step
   process: first the address gets computed (issuing from the RS), and then the
   cache/STQ search occur (issuing from the LDQ). */
void core_exec_DPM_t::LDQ_schedule(void)
{
  struct core_knobs_t * knobs = core->knobs;
  int asid = core->current_thread->asid;
  int i;
  /* walk LDQ, if anyone's ready, issue to DTLB/DL1 */

  int index = LDQ_head;
  for(i=0;i<LDQ_num;i++)
  {
    //int index = (LDQ_head + i) % knobs->exec.LDQ_size;

    /* Load fences */
    if(LDQ[index].uop->decode.is_fence) {
      struct uop_t *fence = LDQ[index].uop;
      if (fence->timing.when_completed != TICK_T_MAX)
        continue;

      /* Only let a fence commit from the head of the LDQ. */
      if (index != LDQ_head) {
        index = modinc(index,knobs->exec.LDQ_size);
        continue;
      }

      /* MFENCE needs to wait until stores prior to it complete */
      if (fence->decode.op == MFENCE_UOP) {
        int stq_ind = LDQ[index].store_color;

        /* Light fence -- the youngest store (at allocation time of the fence)
         * still hasn't gone out of STQ (i.e. gone to caches) */
        if (STQ[stq_ind].std && 
            (STQ[stq_ind].std->decode.uop_seq < fence->decode.uop_seq))
          continue;

        /* Heavy fence -- waiting until the store actually comes back
         * (leaves senior STQ) */
        if (!fence->decode.is_light_fence &&
            (STQ[stq_ind].action_id == LDQ[index].colored_store_action_id))
          continue;
      }

      /* Now let the fence commit */
      zesto_assert(fence->timing.when_completed == TICK_T_MAX,(void)0);
      fence->timing.when_completed = core->sim_cycle;

      index = modinc(index,knobs->exec.LDQ_size);
      continue;
    }

    /* Regular loads */
    if(LDQ[index].addr_valid) /* agen has finished */
    {
      if((!LDQ[index].partial_forward || !partial_forward_throttle) /* load not blocked on partially matching store */ )
      {
        if(LDQ[index].when_issued == TICK_T_MAX) /* load hasn't issued to DL1/STQ already */
        {
          if(check_load_issue_conditions(LDQ[index].uop)) /* retval of true means load is predicted to be cleared for issue */
          {
            struct uop_t * uop = LDQ[index].uop;
            bool send_to_dl1 = (!uop->oracle.is_repeated || (uop->oracle.is_repeated && knobs->memory.DL1_rep_req));
            if(!LDQ[index].first_byte_requested)
            {
              if((cache_enqueuable(core->memory.DTLB, asid, PAGE_TABLE_ADDR(asid, uop->oracle.virt_addr))) &&
                 (!send_to_dl1 || (send_to_dl1 && cache_enqueuable(core->memory.DL1, asid, uop->oracle.virt_addr))) &&
                 (!uop->oracle.is_repeated || (uop->oracle.is_repeated && core->memory.mem_repeater->enqueuable(CACHE_READ, asid, uop->oracle.virt_addr))) &&
                 (port[uop->alloc.port_assignment].STQ->pipe[0].uop == NULL))
              {
                uop->exec.when_data_loaded = TICK_T_MAX;
                if(!uop->oracle.is_sync_op && (uop->exec.when_addr_translated == 0)) {
                  uop->exec.when_addr_translated = TICK_T_MAX;
                  cache_enqueue(core, core->memory.DTLB, NULL, CACHE_READ, asid, uop->Mop->fetch.PC, PAGE_TABLE_ADDR(asid, uop->oracle.virt_addr), uop->exec.action_id, 0, NO_MSHR, uop, DTLB_callback, load_miss_reschedule, NULL, get_uop_action_id);
                }
                else
                  // The wait address is bogus, don't schedule a TLB translation
                  uop->exec.when_addr_translated = core->sim_cycle;

                if(send_to_dl1 && !uop->oracle.is_sync_op)
                  cache_enqueue(core, core->memory.DL1, NULL, CACHE_READ, asid, uop->Mop->fetch.PC, uop->oracle.virt_addr, uop->exec.action_id, 0, NO_MSHR, uop, DL1_callback, load_miss_reschedule, translated_callback, get_uop_action_id);

                if(uop->oracle.is_repeated) {
                  core->memory.mem_repeater->enqueue(uop->oracle.is_sync_op ? CACHE_WAIT : CACHE_READ,
                                   asid, uop->oracle.virt_addr, uop, repeater_callback, get_uop_action_id);
                  LDQ[index].first_repeated = true;
                }
                else
                  LDQ[index].first_repeated = false;

                port[uop->alloc.port_assignment].STQ->pipe[0].uop = uop;
                port[uop->alloc.port_assignment].STQ->pipe[0].action_id = uop->exec.action_id;

                LDQ[index].first_byte_requested = true;
                if(uop->oracle.is_sync_op ||
                   cache_single_line_access(core->memory.DL1, uop->oracle.virt_addr, uop->decode.mem_size))
                {
                  /* not a split-line access */
                  LDQ[index].last_byte_requested = true;
                  /* we just mark the last byte as fetched, so that when the default callback gets invoked
                     and sets first_byte_arrived to true, then it appears that the whole line is available. */
                  LDQ[index].last_byte_arrived = true;
                  LDQ[index].when_issued = core->sim_cycle;
                  LDQ[index].repeater_last_arrived = true;
                  LDQ[index].last_repeated = false;
                }

                if(!LDQ[index].speculative_broadcast) /* need to re-wakeup children */
                {
                  struct odep_t * odep = LDQ[index].uop->exec.odep_uop;
                  if(knobs->exec.payload_depth < core->memory.DL1->latency) /* assume DL1 hit */
                    LDQ[index].uop->timing.when_otag_ready = core->sim_cycle + core->memory.DL1->latency - knobs->exec.payload_depth;
                  else
                    LDQ[index].uop->timing.when_otag_ready = core->sim_cycle;
                  while(odep)
                  {
                    /* odep already finished executing.  Normally this wouldn't (can't?) happen,
                       but there are some weird interactions between cache misses, store-to-load
                       forwarding, and load stalling on earlier partial forwarding cases. */
                    if(odep->uop->timing.when_exec != TICK_T_MAX) /* inst already finished executing */
                    {
                      /* bad mis-scheduling, just flush to recover */
                      core->oracle->pipe_recover(uop->Mop,uop->Mop->oracle.NextPC);
                      ZESTO_STAT(core->stat.num_jeclear++;)
                      if(uop->Mop->oracle.spec_mode)
                        ZESTO_STAT(core->stat.num_wp_jeclear++;)
#ifdef ZTRACE
          ztrace_print(uop,"e|jeclear|UNEXPECTED flush");
#endif
                    }
                    else
                    {
                      odep->uop->timing.when_itag_ready[odep->op_num] = LDQ[index].uop->timing.when_otag_ready;

                      /* put back on to readyQ if appropriate */
                      int j;
                      tick_t when_ready = 0;

                      for(j=0;j<MAX_IDEPS;j++)
                        if(when_ready < odep->uop->timing.when_itag_ready[j])
                          when_ready = odep->uop->timing.when_itag_ready[j];

                      if(when_ready < TICK_T_MAX)
                      {
                        odep->uop->timing.when_ready = when_ready;
                        /* is it possible that the odep is already on the readyq because if
                           it has more than one input replayed on the same cycle, then the
                           first inst to replay will have already placed the odep into
                           the readyq. */
                        if(!odep->uop->exec.in_readyQ)
                          insert_ready_uop(odep->uop);
                      }
                    }

                    odep = odep->next;
                  }
                  LDQ[index].speculative_broadcast = true;
                }
#ifdef ZTRACE
                ztrace_print(uop,"e|load|load enqueued to DL1/DTLB");
#endif
              }
            }

            if(LDQ[index].first_byte_requested && !LDQ[index].last_byte_requested && !uop->oracle.is_sync_op)
            {
              zesto_assert(!uop->oracle.is_sync_op, (void)0);
              md_addr_t split_addr = uop->oracle.virt_addr + uop->decode.mem_size;
              /* split-line access.  XXX: we're currently not handling the 2nd translation
                 for acceses that cross *pages*. */
              if((!send_to_dl1 || (send_to_dl1 && cache_enqueuable(core->memory.DL1, asid, split_addr))) &&
                 (!uop->oracle.is_repeated || (uop->oracle.is_repeated && core->memory.mem_repeater->enqueuable(CACHE_READ, asid, split_addr))))
              {
                if(send_to_dl1) {
                  ZESTO_STAT(core->stat.DL1_load_split_accesses++;)

                  cache_enqueue(core, core->memory.DL1, NULL, CACHE_READ, asid, uop->Mop->fetch.PC, split_addr, uop->exec.action_id, 0, NO_MSHR, uop, DL1_split_callback, load_miss_reschedule, translated_callback, get_uop_action_id, true);
                }
                if(uop->oracle.is_repeated) {
                  core->memory.mem_repeater->enqueue(CACHE_READ, asid, split_addr, uop, repeater_split_callback, get_uop_action_id);
                  LDQ[index].last_repeated = true;
                }
                else
                  LDQ[index].last_repeated = false;
                LDQ[index].last_byte_requested = true;
                LDQ[index].when_issued = core->sim_cycle;
                /* XXX: we should probably do some rescheduling, too */
              }
            }
          }
          else
          {
            LDQ[index].uop->timing.when_otag_ready = TICK_T_MAX;
            LDQ[index].uop->timing.when_completed = TICK_T_MAX;
            LDQ[index].hit_in_STQ = false;

            if(LDQ[index].speculative_broadcast)
            {
              /* we speculatively scheduled children assuming we'd immediately
                 go to cache, but we didn't... so put children back to sleep */
              struct odep_t * odep = LDQ[index].uop->exec.odep_uop;
              while(odep)
              {
                odep->uop->timing.when_itag_ready[odep->op_num] = TICK_T_MAX;
                odep->uop->exec.ivalue_valid[odep->op_num] = false;
#ifdef ZTRACE
                if(odep->uop->timing.when_issued != TICK_T_MAX)
                  ztrace_print(odep->uop,"e|snatch_back|load unable to issue from LDQ to cache");
#endif
                snatch_back(odep->uop);

                odep = odep->next;
              }
              /* clear flag so we don't keep doing this over and over again */
              LDQ[index].speculative_broadcast = false;
            }
          }
        }
      }
    }
    index = modinc(index,knobs->exec.LDQ_size);
  }
}

/* both parts of store have now made it to the STQ: walk
   forward in LDQ to see if there are any loads waiting on this
   this STD.  If so, unblock them. */
void core_exec_DPM_t::ST_ALU_exec(const struct uop_t * const uop)
{
  int idx;
  int num_loads = 0;
  int overwrite_index = uop->alloc.STQ_index;
  struct core_knobs_t * knobs = core->knobs;

  /* XXX using oracle info here. */
  zesto_assert((uop->alloc.STQ_index >= 0) && (uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
  md_addr_t st_addr1 = STQ[uop->alloc.STQ_index].sta->oracle.virt_addr;
  md_addr_t st_addr2 = st_addr1 + STQ[uop->alloc.STQ_index].mem_size - 1;

  /* this is a bit mask for each byte of the stored value; if it reaches zero,
     then we've been completely overwritten and we can stop.
     least-sig bit corresponds to lowest address byte. */
  int overwrite_mask = (1<< STQ[uop->alloc.STQ_index].mem_size)-1;

  for(idx=STQ[uop->alloc.STQ_index].next_load;
      LDQ[idx].uop && (LDQ[idx].uop->decode.uop_seq > uop->decode.uop_seq) && (num_loads < LDQ_num);
      idx=modinc(idx,knobs->exec.LDQ_size))
  {
    if(!LDQ[idx].uop->decode.is_load) {
      num_loads++;
      continue;
    }

    if(LDQ[idx].store_color != uop->alloc.STQ_index) /* some younger stores present */
    {
      /* scan store queue for younger loads to see if we've been overwritten */
      while(overwrite_index != LDQ[idx].store_color)
      {
        overwrite_index = modinc(overwrite_index,knobs->exec.STQ_size); //(overwrite_index + 1) % knobs->exec.STQ_size;
        if(overwrite_index == STQ_tail) {
          zesto_assert(false, (void)0);
          zesto_fatal("searching for matching store color but hit the end of the STQ",(void)0);
        }

        md_addr_t new_st_addr1 = STQ[overwrite_index].virt_addr;
        md_addr_t new_st_addr2 = new_st_addr1 + STQ[overwrite_index].mem_size - 1;

        /* does this store overwrite any of our bytes? 
           (1) address has been computed and
           (2) addr does NOT come completely before or after us */
        if(STQ[overwrite_index].addr_valid &&
          !((st_addr2 < new_st_addr1) || (st_addr1 > new_st_addr2)))
        {
          /* If the old store does a write of 8 bytes at addres 1000, then
             its mask is:  0000000011111111 (with the lsb representing the
             byte at address 1000, and the 8th '1' bit mapping to 1007).
             If the next store is a 2 byte write to address 1002, then
             its initial mask is 00..00011, which gets shifted up to:
             0000000000001100.  We then use this to mask out those two
             bits to get: 0000000011110011, with the remaining 1's indicating
             which bytes are still "in play" (i.e. have not been overwritten
             by a younger store) */
          int new_write_mask = (1<<STQ[overwrite_index].mem_size)-1;
          int offset = new_st_addr1 - st_addr1;
          if(offset < 0) /* new_addr is at lower address */
            new_write_mask >>= (-offset);
          else
            new_write_mask <<= offset;
          overwrite_mask &= ~new_write_mask;

          if(overwrite_mask == 0)
            break; /* while */
        }
      }

      if(overwrite_mask == 0)
        break; /* for */
    }

    if(LDQ[idx].addr_valid && /* if addr not valid, load hasn't finished AGEN... it'll pick up the right value after AGEN */
       (LDQ[idx].when_issued != TICK_T_MAX)) /* similar to addr not valid, the load'll grab the value when it issues from the LDQ */
    {
      md_addr_t ld_addr1 = LDQ[idx].virt_addr;
      md_addr_t ld_addr2 = ld_addr1 + LDQ[idx].mem_size - 1;

      /* order violation/forwarding case/partial forwarding can
         occur if there's any overlap in the addresses */
      if(!((st_addr2 < ld_addr1) || (st_addr1 > ld_addr2)))
      {
        /* Similar to the store-overwrite mask above, we take the store
           mask to see which bytes are still in play (if all bytes overwritten,
           then the overwriting stores would have been responsible for forwarding
           to this load/checking for misspeculations).  Taking the same mask
           as above, if the store wrote to address 1000, and there was that
           one intervening store, the mask would still be 0000000011110011.
           Now if we have a 2-byte load from address 1002, it would generate
           a mask of 00..00011,then shifted to 00..0001100.  When AND'ed with
           the store mask, we would get all zeros indicating no conflict (this
           is ok, even though the store overlaps the load's address range,
           the latter 2-byte store would have forwarded the value to this 2-byte
           load).  However, if the load was say 4-byte read from 09FE, it would
           have a mask of 000..001111.  We would then shift the store's mask
           (since the load is at a lower address) to 00..0001111001100.  ANDing
           these gives 00...001100, which indicates that the 2 least significant
           bytes of the store overlapped with the two most-significant bytes of
           the load, which reveals a match.  Hooray for non-aligned loads and
           stores of every which size! */
        int load_read_mask = (1<<LDQ[idx].mem_size) - 1;
        int offset = ld_addr1 - st_addr1;
        if(offset < 0) /* load is at lower address */
          overwrite_mask <<= (-offset);
        else
          load_read_mask <<= offset;

        if(load_read_mask & overwrite_mask)
        {
          if(LDQ[idx].uop->timing.when_completed != TICK_T_MAX) /* completed --> memory-ordering violation */
          {
            memdep->update(LDQ[idx].uop->Mop->fetch.PC);

            ZESTO_STAT(core->stat.load_nukes++;)
            if(LDQ[idx].uop->Mop->oracle.spec_mode)
              ZESTO_STAT(core->stat.wp_load_nukes++;)

#ifdef ZTRACE
            ztrace_print(uop,"e|order-violation:STQ=%d|store uncovered mis-ordered load",uop->alloc.STQ_index);
            ztrace_print(LDQ[idx].uop,"e|order-violation:LDQ=%d|load squashed",idx);
#endif
            core->oracle->pipe_flush(LDQ[idx].uop->Mop);
          }
          else /* attempted to issue, but STD not available or still in flight in the memory hierarchy */
          {
            /* mark load as schedulable */
            LDQ[idx].when_issued = TICK_T_MAX; /* this will allow it to get issued from LDQ_schedule */
            zesto_assert(LDQ[idx].uop->timing.when_completed == TICK_T_MAX,(void)0);
            LDQ[idx].hit_in_STQ = false; /* it's possible the store commits before the load gets back to searching the STQ */
            LDQ[idx].first_byte_requested = false;
            LDQ[idx].last_byte_requested = false;
            LDQ[idx].first_byte_arrived = false;
            LDQ[idx].last_byte_arrived = false;

            /* invalidate any in-flight loads from cache hierarchy */
            LDQ[idx].uop->exec.action_id = core->new_action_id();

            /* since load attempted to issue, its dependents may have been mis-scheduled */
            if(LDQ[idx].speculative_broadcast)
            {
              LDQ[idx].uop->timing.when_otag_ready = TICK_T_MAX;
              LDQ[idx].uop->timing.when_completed = TICK_T_MAX;
              struct odep_t * odep = LDQ[idx].uop->exec.odep_uop;
              while(odep)
              {
                odep->uop->timing.when_itag_ready[odep->op_num] = TICK_T_MAX;
                odep->uop->exec.ivalue_valid[odep->op_num] = false;
#ifdef ZTRACE
                if(odep->uop->timing.when_issued != TICK_T_MAX)
                  ztrace_print(odep->uop,"e|snatch-back|STA hit, but STD not ready");
#endif
                snatch_back(odep->uop);

                odep = odep->next;
              }
              /* clear flag so we don't keep doing this over and over again */
              LDQ[idx].speculative_broadcast = false;
            }
          }
        }
      }
    }

    num_loads++;
  }
}

void core_exec_DPM_t::STQ_set_addr(struct uop_t * const uop)
{
  zesto_assert((uop->alloc.STQ_index >= 0) && (uop->alloc.STQ_index < core->knobs->exec.STQ_size),(void)0);
  zesto_assert(!STQ[uop->alloc.STQ_index].addr_valid,(void)0);
  STQ[uop->alloc.STQ_index].virt_addr = uop->oracle.virt_addr;
  STQ[uop->alloc.STQ_index].addr_valid = true;
}

void core_exec_DPM_t::STQ_set_data(struct uop_t * const uop)
{
  zesto_assert((uop->alloc.STQ_index >= 0) && (uop->alloc.STQ_index < core->knobs->exec.STQ_size),(void)0);
  zesto_assert(!STQ[uop->alloc.STQ_index].value_valid,(void)0);
  STQ[uop->alloc.STQ_index].value = uop->exec.ovalue;
  STQ[uop->alloc.STQ_index].value_valid = true;
}

/* Process actual execution (in ALUs) of uops, as well as shuffling of
   uops through the payload pipeline. */
void core_exec_DPM_t::ALU_exec(void)
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
        int stage = FU->latency-1;
        struct uop_t * uop = FU->pipe[stage].uop;
        if(uop)
        {
          int squashed = (FU->pipe[stage].action_id != uop->exec.action_id);
          int bypass_available = (port[i].when_bypass_used != core->sim_cycle);
          int needs_bypass = !(uop->decode.is_sta||uop->decode.is_std||uop->decode.is_load||uop->decode.is_ctrl);

#ifdef ZTRACE
          ztrace_print(uop,"e|ALU:squash=%d:needs-bp=%d:bp-available=%d|execution complete",(int)squashed,(int)needs_bypass,(int)bypass_available);
#endif

          if(squashed || !needs_bypass || bypass_available)
          {
            FU->occupancy--;
            zesto_assert(FU->occupancy >= 0,(void)0);
            FU->pipe[stage].uop = NULL;
          }

          /* there's a uop completing execution (that hasn't been squashed) */
          if(!squashed && (!needs_bypass || bypass_available))
          {
            if(needs_bypass)
              port[i].when_bypass_used = core->sim_cycle;

            ZESTO_STAT(core->stat.ROB_writes++;)

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
              int fp_penalty = ((REG_IS_FPR(uop->decode.odep_name) && !(uop->decode.opflags & F_FCOMP)) ||
                               (!REG_IS_FPR(uop->decode.odep_name) && (uop->decode.opflags & F_FCOMP)))?knobs->exec.fp_penalty:0;
              /* TODO: real execute-at-execute of instruction */
              /* XXX for now just copy oracle value */
              uop->exec.ovalue_valid = true;
              uop->exec.ovalue = uop->oracle.ovalue;

              /* alloc, uopQ, and decode all have to search for the
                 recovery point (Mop) because a long flow may have
                 uops that span multiple sections of the pipeline.  */
              if(uop->decode.is_ctrl && (uop->Mop->oracle.NextPC != uop->Mop->fetch.pred_NPC))
              {
                uop->Mop->fetch.pred_NPC = uop->Mop->oracle.NextPC; /* in case this instruction gets flushed more than once (jeclear followed by load-nuke) */
                core->oracle->pipe_recover(uop->Mop,uop->Mop->oracle.NextPC);
                ZESTO_STAT(core->stat.num_jeclear++;)
                if(uop->Mop->oracle.spec_mode)
                  ZESTO_STAT(core->stat.num_wp_jeclear++;)
#ifdef ZTRACE
                ztrace_print(uop,"e|jeclear|branch mispred detected at execute");
#endif
              }
              else if(uop->decode.is_sta)
              {
                STQ_set_addr(uop);
              }
              else if(uop->decode.is_std)
              {
                STQ_set_data(uop);
              }

              if((uop->decode.is_sta || uop->decode.is_std) &&
                  STQ[uop->alloc.STQ_index].addr_valid &&
                  STQ[uop->alloc.STQ_index].value_valid)
              {
                this->ST_ALU_exec(uop);
              }

              zesto_assert(uop->timing.when_completed == TICK_T_MAX,(void)0);
              uop->timing.when_completed = core->sim_cycle+fp_penalty;
              update_last_completed(core->sim_cycle+fp_penalty); /* for deadlock detection */

              /* bypass output value to dependents */
              struct odep_t * odep = uop->exec.odep_uop;
              while(odep)
              {
                zesto_assert(!odep->uop->exec.ivalue_valid[odep->op_num],(void)0);
                odep->uop->exec.ivalue_valid[odep->op_num] = true;
                if(odep->aflags)
                  odep->uop->exec.ivalue[odep->op_num].dw = uop->exec.oflags;
                else
                  odep->uop->exec.ivalue[odep->op_num] = uop->exec.ovalue;
                odep->uop->timing.when_ival_ready[odep->op_num] = core->sim_cycle+fp_penalty;

                odep = odep->next;
              }
            }
          }
        }

        /* shuffle the rest forward */
        if(FU->occupancy > 0)
        {
          for( /*nada*/; stage > 0; stage--)
          {
            if((FU->pipe[stage].uop == NULL) && FU->pipe[stage-1].uop)
            {
              FU->pipe[stage] = FU->pipe[stage-1];
              FU->pipe[stage-1].uop = NULL;
            }
          }
        }
      }
    }
  }

  /* Process Payload RAM pipestages */
  for(i=0;i<knobs->exec.num_exec_ports;i++)
  {
    if(port[i].occupancy > 0)
    {
      int stage = knobs->exec.payload_depth-1;
      struct uop_t * uop = port[i].payload_pipe[stage].uop;
      work_found = true;

      /* uops leaving payload section go to their respective FU's */
      if(uop && (port[i].payload_pipe[stage].action_id == uop->exec.action_id)) /* uop is valid, hasn't been squashed */
      {
        int j;
        int all_ready = true;
        enum md_fu_class FU_class = uop->decode.FU_class;

        /* uop leaves payload regardless of whether it replays */
        port[i].payload_pipe[stage].uop = NULL;
        port[i].occupancy--;
        zesto_assert(port[i].occupancy >= 0,(void)0);

        for(j=0;j<MAX_IDEPS;j++)
          all_ready &= uop->exec.ivalue_valid[j];

        /* have all input values arrived and FU available? */
        if((!all_ready) || (port[i].FU[FU_class]->pipe[0].uop != NULL) || (port[i].FU[FU_class]->when_executable > core->sim_cycle))
        {
          /* if not, replay */
          ZESTO_STAT(core->stat.exec_uops_replayed++;)
          uop->exec.num_replays++;

          for(j=0;j<MAX_IDEPS;j++)
          {
            if((!uop->exec.ivalue_valid[j]) && uop->oracle.idep_uop[j] && (uop->oracle.idep_uop[j]->timing.when_otag_ready < core->sim_cycle))
            {
              uop->timing.when_ready = core->sim_cycle+BIG_LATENCY;
            }
          }
          snatch_back(uop);

          if(uop->timing.when_ready <= core->sim_cycle) /* we were supposed to be ready in the past */
          {
            uop->timing.when_ready = core->sim_cycle+1;
            if(knobs->exec.tornado_breaker)
            {
              /* heuristic replay tornado breaker; if uop is replaying too much, slow down
                 its ability to issue by delaying its readiness (amount of additional delay
                 escalates with the number of times the uop has been replayed).  If a cache
                 access gets "stuck" for some reason (e.g., cache bank blocked due to full
                 MSHRs), the load's dependents won't get notified of a change in the load's
                 latency since the load is still stuck somewhere in the middle of the cache,
                 and so the dependents will enter a loop of scheduling, issuing, reaching
                 execution and discovering that input operands are not ready, and then going
                 back to the RS to immediately start the process all over again.  Such replay
                 "tornados" waste power and issue slots, and so we try to detect and slow
                 down the replay cycle.
                 */
              if(uop->exec.num_replays > 20)
                uop->timing.when_ready += uop->exec.num_replays << 1;
              else if(uop->exec.num_replays > 4)
                uop->timing.when_ready += uop->exec.num_replays << 3;
            }
          }
#ifdef ZTRACE
          ztrace_print(uop,"e|snatch-back|uop cannot go to ALU because inputs not ready",uop->exec.num_replays,uop->timing.when_ready);
#endif

        }
        else
        {
          zesto_assert((uop->alloc.RS_index >= 0) && (uop->alloc.RS_index < knobs->exec.RS_size),(void)0);
          if(uop->decode.in_fusion)
            uop->decode.fusion_head->exec.uops_in_RS--;
#ifdef ZTRACE
          ztrace_print_start(uop,"e|payload|uop goes to ALU");
#endif

          if((!uop->decode.in_fusion) || (uop->decode.fusion_head->exec.uops_in_RS == 0)) /* only deallocate when entire fused uop finished (or not fused) */
          {
            RS[uop->alloc.RS_index] = NULL;
            RS_num--;
            zesto_assert(RS_num >= 0,(void)0);
            if(uop->decode.in_fusion)
            {
              struct uop_t * fusion_uop = uop->decode.fusion_head;
              while(fusion_uop)
              {
                fusion_uop->alloc.RS_index = -1;
                fusion_uop = fusion_uop->decode.fusion_next;
              }
            }
            else
              uop->alloc.RS_index = -1;
#ifdef ZTRACE
            ztrace_print_cont(core->id, ", deallocates from RS");
#endif
          }

#ifdef ZTRACE
          ztrace_print_finish(core->id, "");
#endif

          RS_eff_num--;
          zesto_assert(RS_eff_num >= 0,(void)0);

          /* update port loading table */
          core->alloc->RS_deallocate(uop);

          uop->timing.when_exec = core->sim_cycle;

          /* this port has the proper FU and the first stage is free. */
          zesto_assert(port[i].FU[FU_class] && (port[i].FU[FU_class]->pipe[0].uop == NULL),(void)0);

          port[i].FU[FU_class]->pipe[0].uop = uop;
          port[i].FU[FU_class]->pipe[0].action_id = uop->exec.action_id;
          port[i].FU[FU_class]->occupancy++;
          port[i].FU[FU_class]->when_executable = core->sim_cycle + port[i].FU[FU_class]->issue_rate;
          check_for_work = true;
        }
      }
      else if(uop && (port[i].payload_pipe[stage].action_id != uop->exec.action_id)) /* uop has been squashed */
      {
#ifdef ZTRACE
        ztrace_print(uop,"e|payload|on exit from payload, uop discovered to have been squashed");
#endif
        port[i].payload_pipe[stage].uop = NULL;
        port[i].occupancy--;
        zesto_assert(port[i].occupancy >= 0,(void)0);
      }

      /* shuffle the other uops through the payload pipeline */

      for(/*nada*/; stage > 0; stage--)
        port[i].payload_pipe[stage] = port[i].payload_pipe[stage-1];
      port[i].payload_pipe[0].uop = NULL;
    }
  }

  check_for_work = work_found;
}

void core_exec_DPM_t::recover(const struct Mop_t * const Mop)
{
  /* most flushing/squashing is accomplished through assignments of new action_id's */
}

void core_exec_DPM_t::recover(void)
{
  /* most flushing/squashing is accomplished through assignments of new action_id's */
}

bool core_exec_DPM_t::RS_available(void)
{
  struct core_knobs_t * knobs = core->knobs;
  return RS_num < knobs->exec.RS_size;
}

/* assumes you already called RS_available to check that
   an entry is available */
void core_exec_DPM_t::RS_insert(struct uop_t * const uop)
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
  RS_eff_num++;
  uop->alloc.RS_index = RS_index;

  if(uop->decode.in_fusion)
    uop->exec.uops_in_RS ++; /* used to track when RS can be deallocated */
  uop->alloc.full_fusion_allocated = false;
}

/* add uops to an existing entry (where all uops in the same
   entry are assumed to be fused) */
void core_exec_DPM_t::RS_fuse_insert(struct uop_t * const uop)
{
  /* fusion body shares same RS entry as fusion head */
  uop->alloc.RS_index = uop->decode.fusion_head->alloc.RS_index;
  RS_eff_num++;
  uop->decode.fusion_head->exec.uops_in_RS ++;
  if(uop->decode.fusion_next == NULL)
    uop->decode.fusion_head->alloc.full_fusion_allocated = true;
}

void core_exec_DPM_t::RS_deallocate(struct uop_t * const dead_uop)
{
  struct core_knobs_t * knobs = core->knobs;
  zesto_assert(dead_uop->alloc.RS_index < knobs->exec.RS_size,(void)0);
  if(dead_uop->decode.in_fusion && (dead_uop->timing.when_exec == TICK_T_MAX))
  {
    dead_uop->decode.fusion_head->exec.uops_in_RS --;
    RS_eff_num--;
    zesto_assert(RS_eff_num >= 0,(void)0);
  }
  if(!dead_uop->decode.in_fusion)
  {
    RS_eff_num--;
    zesto_assert(RS_eff_num >= 0,(void)0);
  }

  if(dead_uop->decode.is_fusion_head)
    zesto_assert(dead_uop->exec.uops_in_RS == 0,(void)0);

  if((!dead_uop->decode.in_fusion) || dead_uop->decode.is_fusion_head) /* make head uop responsible for deallocating RS during recovery */
  {
    RS[dead_uop->alloc.RS_index] = NULL;
    RS_num --;
    zesto_assert(RS_num >= 0,(void)0);
  }
  dead_uop->alloc.RS_index = -1;
}

bool core_exec_DPM_t::LDQ_available(void)
{
  struct core_knobs_t * knobs = core->knobs;
  return LDQ_num < knobs->exec.LDQ_size;
}

void core_exec_DPM_t::LDQ_insert(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  //memset(&LDQ[LDQ_tail],0,sizeof(*LDQ));
  memzero(&LDQ[LDQ_tail],sizeof(*LDQ));
  LDQ[LDQ_tail].uop = uop;
  LDQ[LDQ_tail].mem_size = uop->decode.mem_size;
  int store_color = moddec(STQ_tail,knobs->exec.STQ_size); //(STQ_tail - 1 + knobs->exec.STQ_size) % knobs->exec.STQ_size;
  LDQ[LDQ_tail].store_color = store_color;
  if (is_senior_STQ_entry_valid(store_color))
    LDQ[LDQ_tail].colored_store_action_id = STQ[store_color].action_id;
  else
    LDQ[LDQ_tail].colored_store_action_id = core->new_action_id();
  LDQ[LDQ_tail].when_issued = TICK_T_MAX;
  uop->alloc.LDQ_index = LDQ_tail;
  LDQ_num++;
  LDQ_tail = modinc(LDQ_tail,knobs->exec.LDQ_size); //(LDQ_tail+1) % knobs->exec.LDQ_size;
}

/* called by commit */
void core_exec_DPM_t::LDQ_deallocate(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  LDQ[LDQ_head].uop = NULL;
  LDQ_num --;
  LDQ_head = modinc(LDQ_head,knobs->exec.LDQ_size); //(LDQ_head+1) % knobs->exec.LDQ_size;
  uop->alloc.LDQ_index = -1;
}

void core_exec_DPM_t::LDQ_squash(struct uop_t * const dead_uop)
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

bool core_exec_DPM_t::STQ_empty(void)
{
  return STQ_senior_num == 0;
}

bool core_exec_DPM_t::STQ_available(void)
{
  struct core_knobs_t * knobs = core->knobs;
  return STQ_senior_num < knobs->exec.STQ_size;
}

void core_exec_DPM_t::STQ_insert_sta(struct uop_t * const uop)
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
  STQ[STQ_tail].action_id = core->new_action_id();
  uop->alloc.STQ_index = STQ_tail;
  STQ_num++;
  STQ_senior_num++;
  STQ_tail = modinc(STQ_tail,knobs->exec.STQ_size); //(STQ_tail+1) % knobs->exec.STQ_size;
}

void core_exec_DPM_t::STQ_insert_std(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  /* STQ_tail already incremented from the STA.  Just add this uop to STQ->std */
  int index = moddec(STQ_tail,knobs->exec.STQ_size); //(STQ_tail - 1 + knobs->exec.STQ_size) % knobs->exec.STQ_size;
  uop->alloc.STQ_index = index;
  STQ[index].std = uop;
  zesto_assert(STQ[index].sta,(void)0); /* shouldn't have STD w/o a corresponding STA */
  zesto_assert(STQ[index].sta->Mop == uop->Mop,(void)0); /* and we should be from the same macro */
}

void core_exec_DPM_t::STQ_deallocate_sta(void)
{
  STQ[STQ_head].sta = NULL;
}

/* returns true if successful */
bool core_exec_DPM_t::STQ_deallocate_std(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  int asid = core->current_thread->asid;

  bool send_to_dl1 = (!uop->oracle.is_repeated ||
                       (uop->oracle.is_repeated && knobs->memory.DL1_rep_req));
  /* Store write back occurs here at commit.  NOTE: stores go directly to
     DTLB2 (See "Intel 64 and IA-32 Architectures Optimization Reference
     Manual"). */
  struct cache_t * tlb = (core->memory.DTLB2) ? core->memory.DTLB2 : core->memory.DTLB;
  /* Wait until we can submit to DTLB (sync ops don't use TLB) */
  if(get_STQ_request_type(uop) == CACHE_WRITE && 
     !cache_enqueuable(tlb, asid, PAGE_TABLE_ADDR(asid, uop->oracle.virt_addr)))
    return false;
  /* Wait until we can submit to DL1 */
  if(send_to_dl1 && !cache_enqueuable(core->memory.DL1, asid, uop->oracle.virt_addr))
    return false;

  /* Wait until we can submit to repeater */
  if(uop->oracle.is_repeated) {
    if(!core->memory.mem_repeater->enqueuable(get_STQ_request_type(uop), asid, uop->oracle.virt_addr))
    return false;
  }

  /* TODO: Add mfence handling to wait for loads */

  if(!STQ[STQ_head].first_byte_requested)
  {
    STQ[STQ_head].write_complete = false;
    /* These are just dummy placeholders, but we need them
       because the original uop pointers will be made invalid
       when the Mop commits and the oracle reclaims the
       original uop-arrays.  The store continues to occupy STQ
       entries until the store actually finishes accessing
       memory, but commit can proceed past this store once the
       request has entered into the cache hierarchy. */

    /* Send to DL1 */
    if(send_to_dl1) {
      struct uop_t * dl1_uop = core->get_uop_array(1);
      dl1_uop->core = core;
      dl1_uop->alloc.STQ_index = uop->alloc.STQ_index;
      dl1_uop->exec.action_id = STQ[STQ_head].action_id;
      dl1_uop->decode.Mop_seq = uop->decode.Mop_seq;
      dl1_uop->decode.uop_seq = uop->decode.uop_seq;
      dl1_uop->oracle.is_repeated = uop->oracle.is_repeated;
      dl1_uop->oracle.is_sync_op = uop->oracle.is_sync_op;

      cache_enqueue(core, core->memory.DL1, NULL, CACHE_WRITE, asid, uop->Mop->fetch.PC, uop->oracle.virt_addr, dl1_uop->exec.action_id, 0, NO_MSHR, dl1_uop, store_dl1_callback, NULL, store_translated_callback, get_uop_action_id);
    }

    /* Send to DTLB(2), if not a helix signal */
    if(!uop->oracle.is_sync_op) {
      struct uop_t * dtlb_uop = core->get_uop_array(1);
      STQ[STQ_head].translation_complete = false;
      dtlb_uop->core = core;
      dtlb_uop->alloc.STQ_index = uop->alloc.STQ_index;
      dtlb_uop->exec.action_id = STQ[STQ_head].action_id;
      dtlb_uop->decode.Mop_seq = uop->decode.Mop_seq;
      dtlb_uop->decode.uop_seq = uop->decode.uop_seq;

      cache_enqueue(core, tlb, NULL, CACHE_READ, asid, uop->Mop->fetch.PC, PAGE_TABLE_ADDR(asid, uop->oracle.virt_addr), dtlb_uop->exec.action_id, 0, NO_MSHR, dtlb_uop, store_dtlb_callback, NULL, NULL, get_uop_action_id);
    }
    else {
      STQ[STQ_head].translation_complete = true;
    }

    /* Send to memory repeater */
    if(uop->oracle.is_repeated) {
      struct uop_t * rep_uop = core->get_uop_array(1);
      rep_uop->core = core;
      rep_uop->alloc.STQ_index = uop->alloc.STQ_index;
      rep_uop->exec.action_id = STQ[STQ_head].action_id;
      rep_uop->decode.Mop_seq = uop->decode.Mop_seq;
      rep_uop->decode.uop_seq = uop->decode.uop_seq;
      rep_uop->oracle.is_repeated = uop->oracle.is_repeated;
      rep_uop->oracle.is_sync_op = uop->oracle.is_sync_op;

      core->memory.mem_repeater->enqueue(get_STQ_request_type(uop),
                       asid, uop->oracle.virt_addr, rep_uop, repeater_store_callback, get_uop_action_id);
    }

    /* not a split-line access */
    if(uop->oracle.is_sync_op ||
       cache_single_line_access(core->memory.DL1, uop->oracle.virt_addr, uop->decode.mem_size))
    {
      STQ[STQ_head].last_byte_requested = true;
      STQ[STQ_head].last_byte_written = true;
    }

    STQ[STQ_head].first_byte_requested = true;
  }

  /* split-line access */
  if(STQ[STQ_head].first_byte_requested && !STQ[STQ_head].last_byte_requested)
  {
    md_addr_t split_addr = uop->oracle.virt_addr + uop->decode.mem_size;
    zesto_assert(!uop->oracle.is_sync_op, 0);
    /* Wait until we can submit to DL1 */
    if(send_to_dl1 && !cache_enqueuable(core->memory.DL1, asid, split_addr))
      return false;
    /* Wait until we can submit to repeater */
    if(uop->oracle.is_repeated)
      if(!core->memory.mem_repeater->enqueuable(CACHE_WRITE, asid, split_addr))
        return false;

    if(send_to_dl1) {
      ZESTO_STAT(core->stat.DL1_store_split_accesses++;)

      /* Submit second access to DL1 */
      struct uop_t * dl1_split_uop = core->get_uop_array(1);
      dl1_split_uop->core = core;
      dl1_split_uop->alloc.STQ_index = uop->alloc.STQ_index;
      dl1_split_uop->exec.action_id = STQ[STQ_head].action_id;
      dl1_split_uop->decode.Mop_seq = uop->decode.Mop_seq;
      dl1_split_uop->decode.uop_seq = uop->decode.uop_seq;
      dl1_split_uop->oracle.is_repeated = uop->oracle.is_repeated;
      dl1_split_uop->oracle.is_sync_op = uop->oracle.is_sync_op;

      cache_enqueue(core, core->memory.DL1, NULL, CACHE_WRITE, asid, uop->Mop->fetch.PC, split_addr, dl1_split_uop->exec.action_id, 0, NO_MSHR, dl1_split_uop, store_dl1_split_callback, NULL, store_translated_callback, get_uop_action_id);
    }

    /* Submit second access to repeater */
    if(uop->oracle.is_repeated) {
      struct uop_t * rep_split_uop = core->get_uop_array(1);
      rep_split_uop->core = core;
      rep_split_uop->alloc.STQ_index = uop->alloc.STQ_index;
      rep_split_uop->exec.action_id = STQ[STQ_head].action_id;
      rep_split_uop->decode.Mop_seq = uop->decode.Mop_seq;
      rep_split_uop->decode.uop_seq = uop->decode.uop_seq;
      rep_split_uop->oracle.is_repeated = uop->oracle.is_repeated;
      rep_split_uop->oracle.is_sync_op = uop->oracle.is_sync_op;

      core->memory.mem_repeater->enqueue(CACHE_WRITE, asid, split_addr, rep_split_uop, repeater_split_store_callback, get_uop_action_id);
    }

    /* XXX: similar to split-access loads, we're not handling the translation of both
       pages in the case that the access crosses page boundaries. */
    STQ[STQ_head].last_byte_requested = true;
  }

  if(STQ[STQ_head].first_byte_requested && STQ[STQ_head].last_byte_requested)
  {
#ifdef ZTRACE
    ztrace_print(uop,"c|store|store enqueued to DL1/DTLB");
#endif

    STQ[STQ_head].std = NULL;
    STQ_num --;
    STQ_head = modinc(STQ_head,knobs->exec.STQ_size); //(STQ_head+1) % knobs->exec.STQ_size;

    return true;
  }
  else
    return false; /* store not completed processed */
}

void core_exec_DPM_t::STQ_deallocate_senior(void)
{
  struct core_knobs_t * knobs = core->knobs;
  if(STQ[STQ_senior_head].write_complete &&
     STQ[STQ_senior_head].translation_complete)
  {
    STQ[STQ_senior_head].write_complete = false;
    STQ[STQ_senior_head].translation_complete = false;
    STQ[STQ_senior_head].first_byte_requested = false;
    STQ[STQ_senior_head].last_byte_requested = false;
    /* In case request was fullfilled by only one of parallel caches (DL1 and repeater)
     * get a new action_id, to ignore callbacks from the other one */
    STQ[STQ_senior_head].action_id = core->new_action_id();
    STQ_senior_head = modinc(STQ_senior_head,knobs->exec.STQ_size); //(STQ_senior_head + 1) % knobs->exec.STQ_size;
    STQ_senior_num--;
    zesto_assert(STQ_senior_num >= 0,(void)0);
    partial_forward_throttle = false;
  }
}

void core_exec_DPM_t::STQ_squash_sta(struct uop_t * const dead_uop)
{
  struct core_knobs_t * knobs = core->knobs;
  zesto_assert((dead_uop->alloc.STQ_index >= 0) && (dead_uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
  zesto_assert(STQ[dead_uop->alloc.STQ_index].std == NULL,(void)0);
  zesto_assert(STQ[dead_uop->alloc.STQ_index].sta == dead_uop,(void)0);
  //memset(&STQ[dead_uop->alloc.STQ_index],0,sizeof(STQ[0]));
  memzero(&STQ[dead_uop->alloc.STQ_index],sizeof(STQ[0]));
  STQ_num --;
  STQ_senior_num --;
  STQ_tail = moddec(STQ_tail,knobs->exec.STQ_size); //(STQ_tail - 1 + knobs->exec.STQ_size) % knobs->exec.STQ_size;
  zesto_assert(STQ_num >= 0,(void)0);
  zesto_assert(STQ_senior_num >= 0,(void)0);
  dead_uop->alloc.STQ_index = -1;
}

void core_exec_DPM_t::STQ_squash_std(struct uop_t * const dead_uop)
{
  struct core_knobs_t * knobs = core->knobs;
  zesto_assert((dead_uop->alloc.STQ_index >= 0) && (dead_uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
  zesto_assert(STQ[dead_uop->alloc.STQ_index].std == dead_uop,(void)0);
  STQ[dead_uop->alloc.STQ_index].std = NULL;
  dead_uop->alloc.STQ_index = -1;
}

void core_exec_DPM_t::STQ_squash_senior(void)
{
  struct core_knobs_t * knobs = core->knobs;

  while(STQ_senior_num > 0)
  {
    memzero(&STQ[STQ_senior_head],sizeof(*STQ));
    STQ[STQ_senior_head].action_id = core->new_action_id();

    if((STQ_senior_head == STQ_head) && (STQ_num>0))
    {
      STQ_head = modinc(STQ_head,knobs->exec.STQ_size); //(STQ_head + 1) % knobs->exec.STQ_size;
      STQ_num--;
    }
    STQ_senior_head = modinc(STQ_senior_head,knobs->exec.STQ_size); //(STQ_senior_head + 1) % knobs->exec.STQ_size;
    STQ_senior_num--;
  }
}

void core_exec_DPM_t::recover_check_assertions(void)
{
  zesto_assert(STQ_senior_num == 0,(void)0);
  zesto_assert(STQ_num == 0,(void)0);
  zesto_assert(LDQ_num == 0,(void)0);
  zesto_assert(RS_num == 0,(void)0);
}

/* Stores don't write back to cache/memory until commit.  When D$
   and DTLB accesses complete, these functions get called which
   update the status of the corresponding STQ entries.  The STQ
   entry cannot be deallocated until the store has completed. */
void core_exec_DPM_t::store_dl1_callback(void * const op)
{
  struct uop_t * uop = (struct uop_t *)op;
  struct core_t * core = uop->core;
  struct core_exec_DPM_t * E = (core_exec_DPM_t*)core->exec;
  struct core_knobs_t * knobs = core->knobs;

#ifdef ZTRACE
  ztrace_print(uop,"c|store|written to cache/memory");
#endif

  zesto_assert((uop->alloc.STQ_index >= 0) && (uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
  if(!uop->oracle.is_repeated) /* repeater accesses always have precedence */
  {
    if(uop->exec.action_id == E->STQ[uop->alloc.STQ_index].action_id)
    {
      E->STQ[uop->alloc.STQ_index].first_byte_written = true;
      if(E->STQ[uop->alloc.STQ_index].last_byte_written)
        E->STQ[uop->alloc.STQ_index].write_complete = true;
    }
  }
  core->return_uop_array(uop);
}

/* only used for the 2nd part of a split write */
void core_exec_DPM_t::store_dl1_split_callback(void * const op)
{
  struct uop_t * uop = (struct uop_t *)op;
  struct core_t * core = uop->core;
  struct core_exec_DPM_t * E = (core_exec_DPM_t*)core->exec;
  struct core_knobs_t * knobs = core->knobs;

#ifdef ZTRACE
  ztrace_print(uop,"c|store|written to cache/memory");
#endif

  zesto_assert((uop->alloc.STQ_index >= 0) && (uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
  if(!uop->oracle.is_repeated) /* repeater accesses always have precedence */
  {
    if(uop->exec.action_id == E->STQ[uop->alloc.STQ_index].action_id)
    {
      E->STQ[uop->alloc.STQ_index].last_byte_written = true;
      if(E->STQ[uop->alloc.STQ_index].first_byte_written)
        E->STQ[uop->alloc.STQ_index].write_complete = true;
    }
  }
  core->return_uop_array(uop);
}

void core_exec_DPM_t::store_dtlb_callback(void * const op)
{
  struct uop_t * uop = (struct uop_t *)op;
  struct core_t * core = uop->core;
  struct core_exec_DPM_t * E = (core_exec_DPM_t*)core->exec;
  struct core_knobs_t * knobs = core->knobs;
  
#ifdef ZTRACE
  ztrace_print(uop,"c|store|translated");
#endif

  zesto_assert((uop->alloc.STQ_index >= 0) && (uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
  if(uop->exec.action_id == E->STQ[uop->alloc.STQ_index].action_id)
    E->STQ[uop->alloc.STQ_index].translation_complete = true;
  core->return_uop_array(uop);
}

bool core_exec_DPM_t::store_translated_callback(void * const op, const seq_t action_id /* ignored */)
{
  struct uop_t * uop = (struct uop_t *)op;
  struct core_t * core = uop->core;
  struct core_knobs_t * knobs = core->knobs;
  struct core_exec_DPM_t * E = (core_exec_DPM_t*)core->exec;

  if((uop->alloc.STQ_index == -1) || (uop->exec.action_id != E->STQ[uop->alloc.STQ_index].action_id))
    return true;
  zesto_assert((uop->alloc.STQ_index >= 0) && (uop->alloc.STQ_index < knobs->exec.STQ_size),true);
  return E->STQ[uop->alloc.STQ_index].translation_complete;
}

void core_exec_DPM_t::repeater_store_callback(void * const op, bool is_hit)
{
  struct uop_t * uop = (struct uop_t *)op;
  struct core_t * core = uop->core;
  struct core_exec_DPM_t * E = (core_exec_DPM_t*)core->exec;
  struct core_knobs_t * knobs = core->knobs;

#ifdef ZTRACE
  ztrace_print(uop,"c|store|written to repeater");
#endif

  zesto_assert(uop->oracle.is_repeated, (void)0);
  zesto_assert(is_hit, (void)0);
  zesto_assert((uop->alloc.STQ_index >= 0) && (uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
  if(uop->exec.action_id == E->STQ[uop->alloc.STQ_index].action_id)
  {
    if (uop->oracle.is_sync_op)
    {
      core->num_signals_in_pipe--;
      zesto_assert(core->num_signals_in_pipe >= 0, (void)0);
    }
    E->STQ[uop->alloc.STQ_index].first_byte_written = true;
    if(E->STQ[uop->alloc.STQ_index].last_byte_written)
      E->STQ[uop->alloc.STQ_index].write_complete = true;
  }
  core->return_uop_array(uop);
}

/* only used for the 2nd part of a split write */
void core_exec_DPM_t::repeater_split_store_callback(void * const op, bool is_hit)
{
  struct uop_t * uop = (struct uop_t *)op;
  struct core_t * core = uop->core;
  struct core_exec_DPM_t * E = (core_exec_DPM_t*)core->exec;
  struct core_knobs_t * knobs = core->knobs;

#ifdef ZTRACE
  ztrace_print(uop,"c|store|split written to repeater");
#endif

  zesto_assert(uop->oracle.is_repeated, (void)0);
  zesto_assert(is_hit, (void)0);
  zesto_assert((uop->alloc.STQ_index >= 0) && (uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
  if(uop->exec.action_id == E->STQ[uop->alloc.STQ_index].action_id)
  {
    zesto_assert(!uop->oracle.is_sync_op, (void)0);

    E->STQ[uop->alloc.STQ_index].last_byte_written = true;
    if(E->STQ[uop->alloc.STQ_index].first_byte_written)
      E->STQ[uop->alloc.STQ_index].write_complete = true;
  }
  core->return_uop_array(uop);
}

bool core_exec_DPM_t::is_senior_STQ_entry_valid(int STQ_ind)
{
  /* There's a store not yet left for caches */
  if (STQ[STQ_ind].sta || STQ[STQ_ind].std)
    return true;

  /* Else, either empty, or something will come back from caches */
  if (STQ[STQ_ind].first_byte_requested || STQ[STQ_ind].last_byte_requested)
    return true;

  /* Nope, just an empty STQ entry. */
  return false;
}

void core_exec_DPM_t::step()
{
  /* Compatibility: Simulation can call this */
}

void core_exec_DPM_t::exec_fuse_insert(struct uop_t * uop)
{
  fatal("shouldn't be called");
}

void core_exec_DPM_t::exec_insert(struct uop_t * uop)
{
  fatal("shouldn't be called");
}

bool core_exec_DPM_t::port_available(int port_ind)
{
  fatal("shouldn't be called");
}

bool core_exec_DPM_t::exec_fused_ST(struct uop_t * const curr_uop)
{
 fatal("shouldn't be called");
}

bool core_exec_DPM_t::exec_empty(void)
{
  fatal("shouldn't be called");
}
#endif
