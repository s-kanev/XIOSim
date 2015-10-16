/* exec-IO-DPM.cpp - Detailed In-Order Pipeline Model */
/*
 * Derived from Zesto OO model
 * Svilen Kanev, 2011
 */


/* NOTE: For compatibility and interchargability between the IO and OO models
         some structure names are inconsistent with their purpose in an IO pipe.
         Here, portX.payload_pipe is in fact the issue pipe;
               the LDQ and STQ are small buffers and not the big structures of an OO core;
               reservation station (RS) functions are stubs.
               the readyQ is a also a stub and not used.
*/

#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(exec_opt_string,"IO-DPM"))
    return new core_exec_IO_DPM_t(core);
#else

#include <list>
using namespace std;

class core_exec_IO_DPM_t:public core_exec_t
{
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
    counter_t total_occupancy;
  };

  public:

  core_exec_IO_DPM_t(struct core_t * const core);
  virtual void reg_stats(xiosim::stats::StatsDatabase* sdb);
  virtual void freeze_stats(void);
  virtual void update_occupancy(void);
  virtual void reset_execution(void);

  virtual void ALU_exec(void);
  virtual void LDST_exec(void);
  virtual void RS_schedule(void);
  virtual void LDQ_schedule(void);

  virtual void recover(const struct Mop_t * const Mop);
  virtual void recover(void);

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

  /* Stubs in IO pipline -- compatibility only */
  virtual void insert_ready_uop(struct uop_t * const uop);
  virtual bool RS_available(void);
  virtual void RS_insert(struct uop_t * const uop);
  virtual void RS_fuse_insert(struct uop_t * const uop);
  virtual void RS_deallocate(struct uop_t * const uop);


  protected:

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
    struct ALU_t * STQ; /* store-queue lookup/search pipeline for load execution */
    tick_t when_bypass_used; /* to make sure only one inst writes back per cycle, which
                                could happen due to insts with different latencies */
    int num_FU_types; /* the number of FU's bound to this port */
    enum md_fu_class * FU_types; /* the corresponding types of those FUs */
    tick_t when_stalled; /* to make sure stalled uops don't execute more than once */

    counter_t issue_occupancy;
  } * port;
  bool check_for_work; /* used to skip ALU exec when there's no uops */

  struct memdep_t * memdep;


  /* various exec utility functions */

  bool check_load_issue_conditions(const struct uop_t * const uop);

  void load_writeback(struct uop_t * const uop);

  bool can_issue_IO(struct uop_t * const uop);


  void dump_payload();

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

core_exec_IO_DPM_t::core_exec_IO_DPM_t(struct core_t * const arg_core):
  LDQ_head(0), LDQ_tail(0), LDQ_num(0),
  STQ_head(0), STQ_tail(0), STQ_num(0), STQ_senior_num(0),
  STQ_senior_head(0), partial_forward_throttle(false)
{
  struct core_knobs_t * knobs = arg_core->knobs;
  core = arg_core;

  LDQ = (core_exec_IO_DPM_t::LDQ_t*) calloc(knobs->exec.LDQ_size,sizeof(*LDQ));
  if(!LDQ)
    fatal("couldn't calloc LDQ");

  STQ = (core_exec_IO_DPM_t::STQ_t*) calloc(knobs->exec.STQ_size,sizeof(*STQ));
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
      core->memory.DTLB2 = cache_create(core,name,CACHE_READONLY,sets,assoc,1,rp,'w','t','n',banks,1,latency,MSHR_entries,4,1,core->memory.DL2,core->memory.DL2_bus,-1.0); /* on a complete TLB miss, go to the L2 cache to simulate the traffic from a HW page-table walker */
    else
      core->memory.DTLB2 = cache_create(core,name,CACHE_READONLY,sets,assoc,1,rp,'w','t','n',banks,1,latency,MSHR_entries,4,1,uncore->LLC,uncore->LLC_bus,-1.0); /* on a complete TLB miss, go to the LLC to simulate the traffic from a HW page-table walker */
    core->memory.DTLB2->MSHR_cmd_order = NULL;
  }

  /* DTLB */
  if(sscanf(knobs->memory.DTLB_opt_str,"%[^:]:%d:%d:%d:%d:%c:%d",
      name,&sets,&assoc,&banks,&latency, &rp, &MSHR_entries) != 7)
    fatal("invalid DTLB options: <name:sets:assoc:banks:latency:repl-policy:num-MSHR>");

  if(core->memory.DTLB2)
  {
    core->memory.DTLB = cache_create(core,name,CACHE_READONLY,sets,assoc,1,rp,'w','t','n',banks,1,latency,MSHR_entries,4,1,core->memory.DTLB2,core->memory.DTLB_bus,-1.0);
    core->memory.DTLB->MSHR_cmd_order = NULL;
  }
  else
  {
    core->memory.DTLB = cache_create(core,name,CACHE_READONLY,sets,assoc,1,rp,'w','t','n',banks,1,latency,MSHR_entries,4,1,uncore->LLC,uncore->LLC_bus,-1.0);
    core->memory.DTLB->MSHR_cmd_order = NULL;
  }

  core->memory.DTLB->controller = controller_create(knobs->memory.DTLB_controller_opt_str, core, core->memory.DTLB);
  if(core->memory.DTLB2 != NULL)
    core->memory.DTLB2->controller = controller_create(knobs->memory.DTLB2_controller_opt_str, core, core->memory.DTLB2);


  /************************************/
  /* execution port payload pipelines */
  /************************************/
  port = (core_exec_IO_DPM_t::exec_port_t*) calloc(knobs->exec.num_exec_ports,sizeof(*port));
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
core_exec_IO_DPM_t::reg_stats(xiosim::stats::StatsDatabase* sdb)
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
  sprintf(buf,"c%d.load_nukes",arch->id);
  stat_reg_counter(sdb, true, buf, "num pipeflushes due to load-store order violation", &core->stat.load_nukes, 0, TRUE, NULL);
  sprintf(buf,"c%d.wp_load_nukes",arch->id);
  stat_reg_counter(sdb, true, buf, "num pipeflushes due to load-store order violation on wrong-path", &core->stat.wp_load_nukes, 0, TRUE, NULL);
  sprintf(buf,"c%d.DL1_load_split_accesses",arch->id);
  stat_reg_counter(sdb, true, buf, "number of loads requiring split accesses", &core->stat.DL1_load_split_accesses, 0, TRUE, NULL);
  sprintf(buf,"c%d.DL1_load_split_frac",arch->id);
  sprintf(buf2,"c%d.DL1_load_split_accesses/(c%d.DL1.load_lookups-c%d.DL1_load_split_accesses)",arch->id,arch->id,arch->id); /* need to subtract since each split access generated two load accesses */
  stat_reg_formula(sdb, true, buf, "fraction of loads requiring split accesses", buf2, NULL);

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


  for(int i=0; i<core->knobs->exec.num_exec_ports;i++)
  {
    sprintf(buf,"c%d.port%d_issue_occupancy",arch->id, i);
    stat_reg_counter(sdb, true, buf, "total issue occupancy", &port[i].issue_occupancy, 0, TRUE, NULL);
    sprintf(buf, "c%d.port%d_occ_avg",arch->id, i);
    sprintf(buf2,"c%d.port%d_issue_occupancy/c%d.sim_cycle",arch->id,i,arch->id);
    stat_reg_formula(sdb, true, buf, "average issue occupancy", buf2, NULL);
  }

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

void core_exec_IO_DPM_t::freeze_stats(void)
{
  memdep->freeze_stats();
}

void core_exec_IO_DPM_t::update_occupancy(void)
{
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
    port[i].issue_occupancy += port[i].occupancy;

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

void core_exec_IO_DPM_t::reset_execution(void)
{
  struct core_knobs_t * knobs = core->knobs;
  for(int i=0; i<knobs->exec.num_exec_ports; i++)
  {
    port[i].when_stalled = 0;
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

/*****************************/
/* MAIN SCHED/EXEC FUNCTIONS */
/*****************************/

void core_exec_IO_DPM_t::RS_schedule(void) /* for uops in the RS */
{
  /* Compatibility: Simulation can call this */
}

/* returns true if load is allowed to issue (or is predicted to be ok) */
bool core_exec_IO_DPM_t::check_load_issue_conditions(const struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  /* are all previous STA's known? If there's a match, is the STD ready? */
  bool sta_unknown = false;
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
    else if(STQ[i].sta != NULL)
    {
      st_addr1 = STQ[i].sta->oracle.virt_addr; /* addr of first byte */
      sta_unknown = true;
    } else
    {
      num_stores++;
      continue;
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


/* The callback functions below (after load_writeback) mark flags
   in the uop to specify the completion of each task, and only when
   all are done do we call the load-writeback function to finish
   off execution of the load. */

void core_exec_IO_DPM_t::load_writeback(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  zesto_assert((uop->alloc.LDQ_index >= 0) && (uop->alloc.LDQ_index < knobs->exec.LDQ_size),(void)0);
  if(!LDQ[uop->alloc.LDQ_index].hit_in_STQ) /* no match in STQ, so use cache value */
  {
#ifdef ZTRACE
    ztrace_print(uop,"e|load|writeback from cache/writeback");
#endif

    int fp_penalty = REG_IS_IN_FP_UNIT(uop->decode.odep_name)?knobs->exec.fp_penalty:0;

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

//SK - no readyQ
//          if(!odep->uop->exec.in_readyQ)
//            insert_ready_uop(odep->uop);
        }
      }

      /* bypass value */
      zesto_assert(!odep->uop->exec.ivalue_valid[odep->op_num],(void)0);
      // XXX: will get updated, but don't do it prematurely, ignoring fp_penalty
      //odep->uop->exec.ivalue_valid[odep->op_num] = true;
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
void core_exec_IO_DPM_t::DL1_callback(void * const op)
{
  struct uop_t * uop = (struct uop_t*) op;
  class core_exec_IO_DPM_t * E = (core_exec_IO_DPM_t*)uop->core->exec;
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
void core_exec_IO_DPM_t::DL1_split_callback(void * const op)
{
  struct uop_t * uop = (struct uop_t*) op;
  class core_exec_IO_DPM_t * E = (core_exec_IO_DPM_t*)uop->core->exec;
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

void core_exec_IO_DPM_t::repeater_callback(void * const op, bool is_hit)
{
  struct uop_t * uop = (struct uop_t*) op;
  struct core_t * core = uop->core;
  struct core_knobs_t * knobs = core->knobs;
  class core_exec_IO_DPM_t * E = (core_exec_IO_DPM_t*)uop->core->exec;

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
void core_exec_IO_DPM_t::repeater_split_callback(void * const op, bool is_hit)
{
  struct uop_t * uop = (struct uop_t*) op;
  struct core_t * core = uop->core;
  struct core_knobs_t * knobs = core->knobs;
  class core_exec_IO_DPM_t * E = (core_exec_IO_DPM_t*)uop->core->exec;

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

void core_exec_IO_DPM_t::DTLB_callback(void * const op)
{
  struct uop_t * uop = (struct uop_t*) op;
  struct core_t * core = uop->core;
  struct core_exec_IO_DPM_t * E = (core_exec_IO_DPM_t*)uop->core->exec;
  if(uop->alloc.LDQ_index != -1)
  {
    struct LDQ_t * LDQ_item = &E->LDQ[uop->alloc.LDQ_index];
    zesto_assert(uop->exec.when_addr_translated == TICK_T_MAX, (void)0);
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
bool core_exec_IO_DPM_t::translated_callback(void * const op, const seq_t action_id)
{
  struct uop_t * uop = (struct uop_t*) op;
  if((uop->exec.action_id == action_id) && (uop->alloc.LDQ_index != -1))
    return uop->exec.when_addr_translated <= uop->core->sim_cycle;
  else
    return true;
}

/* Used by the cache processing functions to recover the id of the
   uop without needing to know about the uop struct. */
seq_t core_exec_IO_DPM_t::get_uop_action_id(void * const op)
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
void core_exec_IO_DPM_t::load_miss_reschedule(void * const op, const int new_pred_latency)
{
  struct uop_t * uop = (struct uop_t*) op;
  struct core_t * core = uop->core;
  struct core_exec_IO_DPM_t * E = (core_exec_IO_DPM_t*)core->exec;
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
      odep = odep->next;
    }

    /* now assume a hit in this cache level */
    odep = uop->exec.odep_uop;
    if(new_pred_latency != BIG_LATENCY)
      uop->timing.when_otag_ready = core->sim_cycle + new_pred_latency - knobs->exec.payload_depth - 1;

    while(odep)
    {
      odep->uop->timing.when_itag_ready[odep->op_num] = uop->timing.when_otag_ready;

      /* Update operand readiness */
      int j;
      tick_t when_ready = 0;

      for(j=0;j<MAX_IDEPS;j++)
        if(when_ready < odep->uop->timing.when_itag_ready[j])
          when_ready = odep->uop->timing.when_itag_ready[j];

      odep->uop->timing.when_ready = when_ready;
      odep = odep->next;
    }
  }
#ifdef ZTRACE
  else
    ztrace_print(uop,"e|cache-miss|STQ hit so miss not observed");
#endif

}

/* process loads exiting the STQ search pipeline, update caches */
void core_exec_IO_DPM_t::LDST_exec(void)
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
                int fp_penalty = REG_IS_IN_FP_UNIT(uop->decode.odep_name)?knobs->exec.fp_penalty:0;
                uop->exec.ovalue = STQ[j].value;
                uop->exec.ovalue_valid = true;
                //SK - No need to flush here, simply use the found value
                //uop->exec.action_id = core->new_action_id();
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
//SK - no readyQ
//                      if(!odep->uop->exec.in_readyQ)
//                        insert_ready_uop(odep->uop);
                    }
                  }

                  /* bypass output value to dependents */
                  //odep->uop->exec.ivalue_valid[odep->op_num] = true;
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
                    odep->uop->timing.when_ival_ready[odep->op_num] = TICK_T_MAX;
#ifdef ZTRACE
                    if(odep->uop->timing.when_issued != TICK_T_MAX)
                      ztrace_print(odep->uop,"e|snatch-back|parent STQ data not ready");
#endif
                    //SK - Shouldn't be snatching back anything in an IO pipe
                    //snatch_back(odep->uop);

                    odep = odep->next;
                  }
                  /* clear flag so we don't keep doing this over and over again */
                  LDQ[uop->alloc.LDQ_index].speculative_broadcast = false;
                }
                //SK - Don't need to flush uop - we resolve by waiting
                //uop->exec.action_id = core->new_action_id();
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
                odep->uop->timing.when_ival_ready[odep->op_num] = TICK_T_MAX;
#ifdef ZTRACE
                if(odep->uop->timing.when_issued != TICK_T_MAX)
                  ztrace_print(odep->uop,"e|snatch-back|parent had partial match in STQ");
#endif
                //SK - no snatching back of anything in IO
                //snatch_back(odep->uop);
                odep = odep->next;
              }
              /* clear flag so we don't keep doing this over and over again */
              LDQ[uop->alloc.LDQ_index].speculative_broadcast = false;
            }
            //SK - Again, we stall
            //uop->exec.action_id = core->new_action_id();

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
void core_exec_IO_DPM_t::LDQ_schedule(void)
{
  struct core_knobs_t * knobs = core->knobs;
  int asid = core->current_thread->asid;
  int i;
  /* walk LDQ, if anyone's ready, issue to DTLB/DL1 */
  int index = LDQ_head;
  for(i=0;i<LDQ_num;i++)
  {
    //int index = (LDQ_head + i) % knobs->exec.LDQ_size;
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
//XXX: this really shouldn't happen; Now, it happens when a load depends on another load because we don't do checking for ivalue readiness before sending a load. We don't do the check because otherwise hell breaks loose. CHECK CHECK CHECK

//                      fatal("Feel like this shouldn't happen for IO pipe");

                      /* bad mis-scheduling, just flush to recover */
//                      core->oracle->pipe_recover(uop->Mop,uop->Mop->oracle.NextPC);
//                      ZESTO_STAT(core->stat.num_jeclear++;)
//                      if(uop->Mop->oracle.spec_mode)
//                        ZESTO_STAT(core->stat.num_wp_jeclear++;)
#ifdef ZTRACE
//          ztrace_print(uop,"e|jeclear|UNEXPECTED flush");
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
//SK - no readyQ
//                        if(!odep->uop->exec.in_readyQ)
//                          insert_ready_uop(odep->uop);
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
              md_addr_t split_addr = uop->oracle.virt_addr + uop->decode.mem_size;
              zesto_assert(!uop->oracle.is_sync_op, (void)0);
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
                odep->uop->timing.when_ival_ready[odep->op_num] = TICK_T_MAX;
#ifdef ZTRACE
                if(odep->uop->timing.when_issued != TICK_T_MAX)
                  ztrace_print(odep->uop,"e|snatch_back|load unable to issue from LDQ to cache");
#endif
                //snatch_back(odep->uop);

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

void core_exec_IO_DPM_t::STQ_set_addr(struct uop_t * const uop)
{
  zesto_assert(false, (void)0);
}

void core_exec_IO_DPM_t::STQ_set_data(struct uop_t * const uop)
{
  zesto_assert(false, (void)0);
}

//Substituted for ::step
void core_exec_IO_DPM_t::ALU_exec(void)
{
  /* Compatibility: Simulation can call this */
}


void core_exec_IO_DPM_t::recover(const struct Mop_t * const Mop)
{
  //since there is no single structure (ROB, scoreboeard, etc.) that holds all alloced instructions, we need to walk all pipes and later flush, based on program order

  struct core_knobs_t * knobs = core->knobs;

  list<struct uop_t *> flushed_uops;
  list<struct uop_t *>::iterator it;


  /* Flush Functional Units */
  for(int i=0;i<knobs->exec.num_exec_ports;i++)
  {
    for(int j=0;j<port[i].num_FU_types;j++)
    {
      enum md_fu_class FU_type = port[i].FU_types[j];
      struct ALU_t * FU = port[i].FU[FU_type];
      if(FU && (FU->occupancy > 0))
      {
        int stage = FU->latency-1;
        for(;stage>=0; stage--)
        {
          struct uop_t * uop = FU->pipe[stage].uop;

          if(!uop)
            continue;

          /* leave older Mops to go untouched */
          if(uop->decode.Mop_seq <= Mop->oracle.seq)
            continue;

          /* add uop to the flush list observing program order */
          /* this doesn't make any sense in hardware. However, when we clear dependency pointers in the simulator, we should start with the youngest instruction so that we clear everything nicely */
          for(it=flushed_uops.begin(); it!=flushed_uops.end(); it++)
          {
            if((*it)->decode.uop_seq > uop->decode.uop_seq)
              break;
          }

          flushed_uops.insert(it, uop);

          FU->pipe[stage].uop=NULL;
          FU->occupancy--;
        }
      }
    }
  }


  /* flush payload pipe */
  for(int i=0;i<knobs->exec.num_exec_ports;i++)
  {
    if(port[i].occupancy > 0)
    {
      int stage = knobs->exec.payload_depth-1;

      for(;stage>=0; stage--)
      {
         struct uop_t * uop = port[i].payload_pipe[stage].uop;

         if(!uop)
           continue;

         /* leave older Mops to go untouched */
         if(uop->decode.Mop_seq <= Mop->oracle.seq)
           continue;

         /* add uop to the flush list observing program order */
         /* this doesn't make any sense in hardware. However, when we clear dependency pointers in the simulator, we should start with the youngest instruction so that we clear everything nicely */
         for(it=flushed_uops.begin(); it!=flushed_uops.end(); it++)
         {
           if((*it)->decode.uop_seq > uop->decode.uop_seq)
             break;
         }

         flushed_uops.insert(it, uop);

         port[i].payload_pipe[stage].uop = NULL;
         port[i].occupancy--;
      }
    }

//XXX: some cases break this (REP'd instruction that spans to the issue pipe)
//    zesto_assert(port[i].occupancy == 0, (void)0);
  }


  /* this pretty much clears dependency pointers and LD/ST queues */
  /* should be called in program order; XXX: move to oracle??? */
  for(it=flushed_uops.begin(); it!=flushed_uops.end(); it++)
  {
     core->commit->squash_uop(*it);
  }

}

/* not that different from the overloaded version above */
void core_exec_IO_DPM_t::recover(void)
{
  struct core_knobs_t * knobs = core->knobs;

  list<struct uop_t *> flushed_uops;
  list<struct uop_t *>::iterator it;


  /* Flush Functional Units */
  for(int i=0;i<knobs->exec.num_exec_ports;i++)
  {
    for(int j=0;j<port[i].num_FU_types;j++)
    {
      enum md_fu_class FU_type = port[i].FU_types[j];
      struct ALU_t * FU = port[i].FU[FU_type];
      if(FU && (FU->occupancy > 0))
      {
        int stage = FU->latency-1;
        for(;stage>=0; stage--)
        {
          struct uop_t * uop = FU->pipe[stage].uop;

          if(!uop)
            continue;

          /* add uop to the flush list observing program order */
          /* this doesn't make any sense in hardware. However, when we clear dependency pointers in the simulator, we should start with the youngest instruction so that we clear everything nicely */
          for(it=flushed_uops.begin(); it!=flushed_uops.end(); it++)
          {
            if((*it)->decode.uop_seq > uop->decode.uop_seq)
              break;
          }

          flushed_uops.insert(it, uop);

          FU->pipe[stage].uop=NULL;
          FU->occupancy--;
        }
      }
      zesto_assert(FU->occupancy == 0, (void)0);
    }
  }


  /* flush payload pipe */
  for(int i=0;i<knobs->exec.num_exec_ports;i++)
  {
    if(port[i].occupancy > 0)
    {
      int stage = knobs->exec.payload_depth-1;

      for(;stage>=0; stage--)
      {
         struct uop_t * uop = port[i].payload_pipe[stage].uop;

         if(!uop)
           continue;

         /* add uop to the flush list observing program order */
         /* this doesn't make any sense in hardware. However, when we clear dependency pointers in the simulator, we should start with the oldest instruction so that we clear everything nicely */
         for(it=flushed_uops.begin(); it!=flushed_uops.end(); it++)
         {
           if((*it)->decode.uop_seq > uop->decode.uop_seq)
             break;
         }

         flushed_uops.insert(it, uop);

         port[i].payload_pipe[stage].uop = NULL;
         port[i].occupancy--;
      }
    }

    zesto_assert(port[i].occupancy == 0, (void)0);
  }


  /* this pretty much clears dependency pointers and LD/ST queus */
  /* should be called in program order; XXX: move to oracle??? */
  for(it=flushed_uops.begin(); it!=flushed_uops.end(); it++)
  {
     core->commit->squash_uop(*it);
  }

  recover_check_assertions();
}


void core_exec_IO_DPM_t::insert_ready_uop(struct uop_t * const uop)
{
   fatal("Not implemented!");
}

bool core_exec_IO_DPM_t::RS_available(void)
{
   fatal("Not implemented!");
}

bool core_exec_IO_DPM_t::port_available(int port_ind)
{
   return (port[port_ind].payload_pipe[0].uop == NULL);
}


bool core_exec_IO_DPM_t::exec_empty(void)
{
  struct core_knobs_t * knobs = core->knobs;
  for(int i=0;i<knobs->exec.num_exec_ports;i++)
  {
    if(port[i].occupancy > 0)
      return false;

    for(int j=0;j<port[i].num_FU_types;j++)
    {
      enum md_fu_class FU_type = port[i].FU_types[j];
      struct ALU_t * FU = port[i].FU[FU_type];
      if(FU && (FU->occupancy > 0))
        return false;
    }

  }

  return true;
}

/* Insert uop straight to appropriate execution port */
void core_exec_IO_DPM_t::exec_insert(struct uop_t * const uop)
{
  int port_ind = uop->alloc.port_assignment;
  zesto_assert(port[port_ind].payload_pipe[0].uop == NULL, (void)0);

  port[port_ind].payload_pipe[0].uop = uop;
  port[port_ind].payload_pipe[0].action_id = uop->exec.action_id;
  port[port_ind].occupancy++;

  /* for consistency: */
  uop->alloc.RS_index = -1;
  uop->exec.in_readyQ = false;

  uop->alloc.full_fusion_allocated = false;
}

void core_exec_IO_DPM_t::RS_insert(struct uop_t * const uop)
{
  fatal("Not implemented!");
}


/* add uops to an existing entry (where all uops in the same
   entry are assumed to be fused) */
void core_exec_IO_DPM_t::exec_fuse_insert(struct uop_t * const uop)
{
  zesto_assert(uop->decode.in_fusion, (void)0);
  zesto_assert(uop->alloc.port_assignment == uop->decode.fusion_head->alloc.port_assignment, (void)0);
  uop->alloc.RS_index = uop->decode.fusion_head->alloc.RS_index;

  if(uop->decode.fusion_next == NULL)
    uop->decode.fusion_head->alloc.full_fusion_allocated = true;
}

void core_exec_IO_DPM_t::RS_fuse_insert(struct uop_t * const uop)
{
  fatal("Not implemented!");
}

void core_exec_IO_DPM_t::RS_deallocate(struct uop_t * const dead_uop)
{
  fatal("Not implemented!");
}

//SK - called in alloc
bool core_exec_IO_DPM_t::LDQ_available(void)
{
  struct core_knobs_t * knobs = core->knobs;
  return LDQ_num < knobs->exec.LDQ_size;
}

//SK - called after AGEN in payload pipe
void core_exec_IO_DPM_t::LDQ_insert(struct uop_t * const uop)
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
  zesto_assert(LDQ_tail >=0, (void)0);
}

/* called by the end of payload pipe */
void core_exec_IO_DPM_t::LDQ_deallocate(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  zesto_assert(LDQ[LDQ_head].uop == uop, (void)0);
  LDQ[LDQ_head].uop = NULL;
  LDQ_num --;
  LDQ_head = modinc(LDQ_head,knobs->exec.LDQ_size); //(LDQ_head+1) % knobs->exec.LDQ_size;
//To be removed
  zesto_assert(LDQ_head >= 0, (void)0);
  uop->alloc.LDQ_index = -1;
}

void core_exec_IO_DPM_t::LDQ_squash(struct uop_t * const dead_uop)
{
  struct core_knobs_t * knobs = core->knobs;
  zesto_assert((dead_uop->alloc.LDQ_index >= 0) && (dead_uop->alloc.LDQ_index < knobs->exec.LDQ_size),(void)0);
  zesto_assert(LDQ[dead_uop->alloc.LDQ_index].uop == dead_uop,(void)0);
  //memset(&LDQ[dead_uop->alloc.LDQ_index],0,sizeof(LDQ[0]));
  memzero(&LDQ[dead_uop->alloc.LDQ_index],sizeof(LDQ[0]));
  LDQ_num --;
  LDQ_tail = moddec(LDQ_tail,knobs->exec.LDQ_size); //(LDQ_tail - 1 + knobs->exec.LDQ_size) % knobs->exec.LDQ_size;
  zesto_assert(LDQ_tail >= 0, (void)0);
  zesto_assert(LDQ_num >= 0,(void)0);
  dead_uop->alloc.LDQ_index = -1;
}

bool core_exec_IO_DPM_t::STQ_empty(void)
{
  return STQ_senior_num == 0;
}

bool core_exec_IO_DPM_t::STQ_available(void)
{
  struct core_knobs_t * knobs = core->knobs;
  return STQ_senior_num < knobs->exec.STQ_size;
}

void core_exec_IO_DPM_t::STQ_insert_sta(struct uop_t * const uop)
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
  STQ_senior_num++;
  STQ_tail = modinc(STQ_tail,knobs->exec.STQ_size); //(STQ_tail+1) % knobs->exec.STQ_size;
}

void core_exec_IO_DPM_t::STQ_insert_std(struct uop_t * const uop)
{

 struct core_knobs_t * knobs = core->knobs;
  /* STQ_tail already incremented from the STA.  Just add this uop to STQ->std */
  int index = moddec(STQ_tail,knobs->exec.STQ_size); //(STQ_tail - 1 + knobs->exec.STQ_size) % knobs->exec.STQ_size;
  uop->alloc.STQ_index = index;
  STQ[index].std = uop;
  zesto_assert(STQ[index].sta,(void)0); /* shouldn't have STD w/o a corresponding STA */
  zesto_assert(STQ[index].sta->Mop == uop->Mop,(void)0); /* and we should be from the same macro */
}

void core_exec_IO_DPM_t::STQ_deallocate_sta(void)
{
  if(STQ[STQ_head].sta != NULL)
    STQ[STQ_head].sta->alloc.STQ_index = -1;
  STQ[STQ_head].sta = NULL;
}

/* returns true if successful */
bool core_exec_IO_DPM_t::STQ_deallocate_std(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;
  int asid = core->current_thread->asid;
  bool send_to_dl1 = (!uop->oracle.is_repeated ||
                       (uop->oracle.is_repeated && knobs->memory.DL1_rep_req));
  /* Store write back occurs here at commit.  NOTE: stores go directly to
     DTLB2 (See "Intel 64 and IA-32 Architectures Optimization Reference
     Manual"). */
  struct cache_t * tlb = (core->memory.DTLB2)?core->memory.DTLB2:core->memory.DTLB;
  /* Wait until we can submit to DTLB */
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

  if(!STQ[STQ_head].first_byte_requested)
  {
    STQ[STQ_head].write_complete = false;
    /* The uop structs are just dummy placeholders, but we need them
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
      STQ[STQ_head].action_id = core->new_action_id();
      dl1_uop->exec.action_id = STQ[STQ_head].action_id;
      dl1_uop->decode.Mop_seq = uop->decode.Mop_seq;
      dl1_uop->decode.uop_seq = uop->decode.uop_seq;
      dl1_uop->oracle.is_repeated = uop->oracle.is_repeated;
      dl1_uop->oracle.is_sync_op = uop->oracle.is_sync_op;

      cache_enqueue(core,core->memory.DL1, NULL, CACHE_WRITE, asid, uop->Mop->fetch.PC, uop->oracle.virt_addr, dl1_uop->exec.action_id, 0, NO_MSHR, dl1_uop, store_dl1_callback, NULL, store_translated_callback, get_uop_action_id);
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

      cache_enqueue(core,tlb, NULL, CACHE_READ, asid, uop->Mop->fetch.PC, PAGE_TABLE_ADDR(asid, uop->oracle.virt_addr), dtlb_uop->exec.action_id, 0, NO_MSHR, dtlb_uop, store_dtlb_callback, NULL, NULL, get_uop_action_id);
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
    ztrace_print(uop,"c|store|store enqueued to DL1/DTLB/repeater");
#endif

    if(STQ[STQ_head].std != NULL)
      STQ[STQ_head].std->alloc.STQ_index = -1;
    STQ[STQ_head].std = NULL;
    STQ_num --;
    STQ_head = modinc(STQ_head,knobs->exec.STQ_size); //(STQ_head+1) % knobs->exec.STQ_size;

    return true;
  }
  else
    return false; /* store not completed processed */
}

void core_exec_IO_DPM_t::STQ_deallocate_senior(void)
{
  struct core_knobs_t * knobs = core->knobs;
  if(STQ[STQ_senior_head].write_complete &&
     STQ[STQ_senior_head].translation_complete)
  {
    STQ[STQ_senior_head].write_complete = false;
    STQ[STQ_senior_head].translation_complete = false;
    /* In case request was fullfilled by only one of parallel caches (DL1 and repeater)
     * get a new action_id, to ignore callbacks from the other one */
    STQ[STQ_senior_head].action_id = core->new_action_id();
    STQ_senior_head = modinc(STQ_senior_head,knobs->exec.STQ_size); //(STQ_senior_head + 1) % knobs->exec.STQ_size;
    STQ_senior_num--;
    zesto_assert(STQ_senior_num >= 0,(void)0);
    partial_forward_throttle = false;
  }
}

void core_exec_IO_DPM_t::STQ_squash_sta(struct uop_t * const dead_uop)
{
  struct core_knobs_t * knobs = core->knobs;
  zesto_assert((dead_uop->alloc.STQ_index >= 0) && (dead_uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
  zesto_assert(STQ[dead_uop->alloc.STQ_index].sta == dead_uop,(void)0);
  STQ[dead_uop->alloc.STQ_index].sta = NULL;
  dead_uop->alloc.STQ_index = -1;
}

void core_exec_IO_DPM_t::STQ_squash_std(struct uop_t * const dead_uop)
{
  struct core_knobs_t * knobs = core->knobs;
  zesto_assert((dead_uop->alloc.STQ_index >= 0) && (dead_uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
  zesto_assert(STQ[dead_uop->alloc.STQ_index].sta == NULL,(void)0);
  zesto_assert(STQ[dead_uop->alloc.STQ_index].std == dead_uop,(void)0);
  //memset(&STQ[dead_uop->alloc.STQ_index],0,sizeof(STQ[0]));
  memzero(&STQ[dead_uop->alloc.STQ_index],sizeof(STQ[0]));
  STQ_num --;
  STQ_senior_num --;
  STQ_tail = moddec(STQ_tail,knobs->exec.STQ_size); //(STQ_tail - 1 + knobs->exec.STQ_size) % knobs->exec.STQ_size;
  zesto_assert(STQ_num >= 0,(void)0);
  zesto_assert(STQ_senior_num >= 0,(void)0);
  dead_uop->alloc.STQ_index = -1;
}

void core_exec_IO_DPM_t::STQ_squash_senior(void)
{
  struct core_knobs_t * knobs = core->knobs;

  while(STQ_senior_num > 0)
  {
    /* Most of the time, instructions should be invalid here. But some flows get a vlid uop in senior queue, which should be disposed of properly, without leaving a dangling STQ pointer */
    if(STQ[STQ_senior_head].sta != NULL)
      STQ[STQ_senior_head].sta->alloc.STQ_index = -1;

    if(STQ[STQ_senior_head].std != NULL)
      STQ[STQ_senior_head].std->alloc.STQ_index = -1;

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

void core_exec_IO_DPM_t::recover_check_assertions(void)
{
  zesto_assert(STQ_senior_num == 0,(void)0);
  zesto_assert(STQ_num == 0,(void)0);
  zesto_assert(LDQ_num == 0,(void)0);
}

/* Stores don't write back to cache/memory until commit.  When D$
   and DTLB accesses complete, these functions get called which
   update the status of the corresponding STQ entries.  The STQ
   entry cannot be deallocated until the store has completed. */
void core_exec_IO_DPM_t::store_dl1_callback(void * const op)
{
  struct uop_t * uop = (struct uop_t *)op;
  struct core_t * core = uop->core;
  struct core_exec_IO_DPM_t * E = (core_exec_IO_DPM_t*)core->exec;
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
      if(E->STQ[uop->alloc.STQ_index].last_byte_written) {
        E->STQ[uop->alloc.STQ_index].write_complete = true;
        E->update_last_completed(core->sim_cycle);
      }
    }
  }
  core->return_uop_array(uop);
}

/* only used for the 2nd part of a split write */
void core_exec_IO_DPM_t::store_dl1_split_callback(void * const op)
{
  struct uop_t * uop = (struct uop_t *)op;
  struct core_t * core = uop->core;
  struct core_exec_IO_DPM_t * E = (core_exec_IO_DPM_t*)core->exec;
  struct core_knobs_t * knobs = core->knobs;

#ifdef ZTRACE
  ztrace_print(uop,"c|store|split written to cache/memory");
#endif

  zesto_assert((uop->alloc.STQ_index >= 0) && (uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
  if(!uop->oracle.is_repeated) /* repeater accesses always have precedence */
  {
    if(uop->exec.action_id == E->STQ[uop->alloc.STQ_index].action_id)
    {
      E->STQ[uop->alloc.STQ_index].last_byte_written = true;
      if(E->STQ[uop->alloc.STQ_index].first_byte_written) {
        E->STQ[uop->alloc.STQ_index].write_complete = true;
        E->update_last_completed(core->sim_cycle);
      }
    }
  }
  core->return_uop_array(uop);
}

void core_exec_IO_DPM_t::store_dtlb_callback(void * const op)
{
  struct uop_t * uop = (struct uop_t *)op;
  struct core_t * core = uop->core;
  struct core_exec_IO_DPM_t * E = (core_exec_IO_DPM_t*)core->exec;
  struct core_knobs_t * knobs = core->knobs;

#ifdef ZTRACE
  ztrace_print(uop,"c|store|translated");
#endif

  zesto_assert((uop->alloc.STQ_index >= 0) && (uop->alloc.STQ_index < knobs->exec.STQ_size),(void)0);
  if(uop->exec.action_id == E->STQ[uop->alloc.STQ_index].action_id)
    E->STQ[uop->alloc.STQ_index].translation_complete = true;
  core->return_uop_array(uop);
}

bool core_exec_IO_DPM_t::store_translated_callback(void * const op, const seq_t action_id /* ignored */)
{
  struct uop_t * uop = (struct uop_t *)op;
  struct core_t * core = uop->core;
  struct core_knobs_t * knobs = core->knobs;
  struct core_exec_IO_DPM_t * E = (core_exec_IO_DPM_t*)core->exec;

  if((uop->alloc.STQ_index == -1) || (uop->exec.action_id != E->STQ[uop->alloc.STQ_index].action_id))
    return true;
  zesto_assert((uop->alloc.STQ_index >= 0) && (uop->alloc.STQ_index < knobs->exec.STQ_size),true);
  return E->STQ[uop->alloc.STQ_index].translation_complete;
}

void core_exec_IO_DPM_t::repeater_store_callback(void * const op, bool is_hit)
{
  struct uop_t * uop = (struct uop_t *)op;
  struct core_t * core = uop->core;
  struct core_exec_IO_DPM_t * E = (core_exec_IO_DPM_t*)core->exec;
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
    if(E->STQ[uop->alloc.STQ_index].last_byte_written) {
      E->STQ[uop->alloc.STQ_index].write_complete = true;
      E->update_last_completed(core->sim_cycle);
    }
  }
  core->return_uop_array(uop);
}

/* only used for the 2nd part of a split write */
void core_exec_IO_DPM_t::repeater_split_store_callback(void * const op, bool is_hit)
{
  struct uop_t * uop = (struct uop_t *)op;
  struct core_t * core = uop->core;
  struct core_exec_IO_DPM_t * E = (core_exec_IO_DPM_t*)core->exec;
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
    if(E->STQ[uop->alloc.STQ_index].first_byte_written) {
      E->STQ[uop->alloc.STQ_index].write_complete = true;
      E->update_last_completed(core->sim_cycle);
    }
  }
  core->return_uop_array(uop);
}


void core_exec_IO_DPM_t::dump_payload()
{
  struct core_knobs_t * knobs = core->knobs;
  struct uop_t * uop;
  printf("cycle: %d\n",(int)core->sim_cycle);
  for(int i=0; i<knobs->exec.num_exec_ports; i++){
    printf("%d:|",i);
    for(int stage=0; stage<knobs->exec.payload_depth; stage++)
    {
     uop = port[i].payload_pipe[stage].uop;
     if(uop)
      {
        if(uop->decode.fusion_next)
          printf(" %s \t|", md_op2name[uop->decode.fusion_next->decode.op]);
        else
          printf(" %s \t|", md_op2name[uop->decode.op]);
      }
      else
        printf(" \t|");
    }
    printf("\n");
  }
}

void core_exec_IO_DPM_t::step()
{
  struct core_knobs_t * knobs = core->knobs;
  int i;

// FIXME: Re-enable for performance
//  if(check_for_work == false)
//    return;

  bool work_found = false;
  bool exec_stall = false;

  /* Search for uops completing execution and order them according to program order*/

  list<struct uop_t *> executed_uops;
  list<struct uop_t *>::iterator it;

  for(i=0;i<knobs->exec.num_exec_ports;i++)
  {
    struct uop_t * uop;
    for(int j=0;j<port[i].num_FU_types;j++)
    {
       enum md_fu_class FU_type = port[i].FU_types[j];
       struct ALU_t * FU = port[i].FU[FU_type];
       if(FU && (FU->occupancy > 0))
       {
          int stage = FU->latency-1;
          uop = FU->pipe[stage].uop;

          if(!uop)
            continue;

          for(it=executed_uops.begin(); it!=executed_uops.end(); it++)
          {
            if((*it)->decode.uop_seq > uop->decode.uop_seq)
              break;
          }

          executed_uops.insert(it, uop);
       }
    }

    uop = port[i].payload_pipe[knobs->exec.payload_depth-1].uop;
    if(uop && port[i].payload_pipe[knobs->exec.payload_depth-1].action_id == uop->exec.action_id)
    {
      if(uop->decode.in_fusion && uop->decode.is_load && uop->exec.ovalue_valid)
        uop = uop->decode.fusion_next;

      if((uop->decode.is_load && uop->exec.ovalue_valid) || uop->decode.is_nop ||
           uop->Mop->decode.is_trap || ((uop->decode.opflags & F_AGEN) == F_AGEN) ||
           uop->decode.is_sta || uop->decode.is_std)
      {
         if(can_issue_IO(uop))
         {

            for(it=executed_uops.begin(); it!=executed_uops.end(); it++)
            {
              if((*it)->decode.uop_seq > uop->decode.uop_seq)
                break;
            }
            executed_uops.insert(it, uop);
         }
         else
         {
            uop->exec.num_replays++;
            ZESTO_STAT(core->stat.exec_uops_replayed++;)

            if(port[i].when_stalled == 0)
              port[i].when_stalled = core->sim_cycle;
         }
      }
      else port[i].when_stalled = 0;
    }
  }

  work_found = !executed_uops.empty();


  for(it=executed_uops.begin(); it!=executed_uops.end(); it++)
  {
    /* process last stage of FU pipeline (those uops completing execution) */
    struct uop_t * uop = *it;
    i = uop->alloc.port_assignment;


    //uop is leaving the end of the issue pipe
    if(uop->decode.is_load || uop->decode.is_nop ||
         uop->Mop->decode.is_trap || ((uop->decode.opflags & F_AGEN) == F_AGEN) ||
         uop->decode.is_sta || uop->decode.is_std)
    {
       /* This may happen when we process a jump prior in program order on the same cycle
        (flushing of instruction is already taken care of, but executed_uops (the local cache) isn't updated)*/
       if(port[i].payload_pipe[knobs->exec.payload_depth-1].uop != uop)
         continue;

       if(!core->commit->pre_commit_available())
       {
          ZESTO_STAT(core->stat.exec_uops_replayed++;)
          uop->exec.num_replays++;
          //stall port of issue pipe

          //stall = true;
          if(port[i].when_stalled == 0)
            port[i].when_stalled = core->sim_cycle;

//          break;
       }
       else
       {
          port[i].when_stalled = 0;
          port[i].payload_pipe[knobs->exec.payload_depth-1].uop = NULL;
          port[i].occupancy--;
          zesto_assert(port[i].occupancy >= 0, (void)0);

#ifdef ZTRACE
          ztrace_print(uop, "e|payload|going straight to commit");
#endif

          if(uop->decode.is_nop || uop->Mop->decode.is_trap)
          {
             uop->timing.when_exec = core->sim_cycle;
             uop->timing.when_issued = core->sim_cycle;
             uop->timing.when_completed = core->sim_cycle;
          }

          if((!uop->decode.in_fusion) || uop->decode.is_fusion_head)
             core->commit->pre_commit_insert(uop);
          else
             core->commit->pre_commit_fused_insert(uop);

          /* loads can be safely removed from load queue */
          if(uop->decode.is_load)
          {
             zesto_assert(uop->exec.ovalue_valid, (void)0);
             LDQ_deallocate(uop);
          }
       }
    }

    //uop is leaving a FU
    else
    {
      struct ALU_t * FU = port[i].FU[uop->decode.FU_class];
      int stage = FU->latency-1;


      /* This may happen when we process a jump prior in program order on the same cycle
       (flushing of instruction is already taken care of, but executed_uops (the local cache) isn't updated)*/
      if(FU->pipe[stage].uop != uop)
         continue;

      int squashed = (FU->pipe[stage].action_id != uop->exec.action_id);
#ifdef ZTRACE
      int bypass_available = (port[i].when_bypass_used != core->sim_cycle);
#endif
      /* SK - TODO: may need to rethink for atomic uops */
      int needs_bypass = !(uop->decode.is_sta||uop->decode.is_std||uop->decode.is_load||uop->decode.is_ctrl);

#ifdef ZTRACE
      ztrace_print(uop, "e|ALU:squash=%d:needs-bp=%d:bp-available=%d|execution complete",(int)squashed,(int)needs_bypass,(int)bypass_available);
#endif

      if(squashed)// || !needs_bypass || bypass_available)
      {
         FU->occupancy--;
         zesto_assert(FU->occupancy >= 0,(void)0);
         FU->pipe[stage].uop = NULL;
      }

      bool uop_goes_to_commit = true;

      if(uop->decode.in_fusion && uop->decode.fusion_next && !uop->decode.fusion_next->decode.is_sta)
         uop_goes_to_commit = false;

      if(uop_goes_to_commit && !core->commit->pre_commit_available())
      {
         exec_stall = true;
         for(int k=0;k<knobs->exec.num_exec_ports;k++)
           if(port[k].when_stalled == 0)
             port[k].when_stalled = core->sim_cycle;
 //        break;
      }
      else
      {

      /* there's uop completing execution (hasn't been squashed) */
      if(!squashed)// && (!needs_bypass || bypass_available))
      {
         int fp_penalty = ((REG_IS_IN_FP_UNIT(uop->decode.odep_name) && !(uop->decode.opflags & F_FCOMP)) ||
                          (!REG_IS_IN_FP_UNIT(uop->decode.odep_name) && (uop->decode.opflags & F_FCOMP)))?knobs->exec.fp_penalty:0;


         if(uop->timing.when_completed == TICK_T_MAX)
         {
            uop->timing.when_completed = core->sim_cycle+fp_penalty;
            update_last_completed(core->sim_cycle+fp_penalty); /* for deadlock detection*/
         }
         //when_completed can only be set if waiting for fp_penalty
         else
            zesto_assert(fp_penalty > 0, (void)0);

         //Wait until fp_penalty has been paid
         if(core->sim_cycle < uop->timing.when_completed)
         {
         }
         else
         {
           if(needs_bypass)
            port[i].when_bypass_used = core->sim_cycle;

           /* actual execution occurs here - copy oracle value*/
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


           /* bypass output value to dependents */
           struct odep_t * odep = uop->exec.odep_uop;
           while(odep)
           {
  //XXX:assert breaks on exec_stall, fix repeating!
  //         zesto_assert(!odep->uop->exec.ivalue_valid[odep->op_num], (void)0);

             //XXX: Disable so we account for fp_penalty correctly
             //odep->uop->exec.ivalue_valid[odep->op_num] = true;
             if(odep->aflags)
                odep->uop->exec.ivalue[odep->op_num].dw = uop->exec.oflags;
             else
                odep->uop->exec.ivalue[odep->op_num] = uop->exec.ovalue;
             odep->uop->timing.when_ival_ready[odep->op_num] = core->sim_cycle+fp_penalty;
             odep = odep->next;
           }

           /* if we are in the middle of a fusion, don't alloc pre_commit yet until the last part (apart from STs) of the fusion is completed - workaround for OP-x-OP fusion */
//         bool uop_goes_to_commit = true;

//         if(uop->decode.in_fusion && uop->decode.fusion_next && !uop->decode.fusion_next->decode.is_sta)
//            uop_goes_to_commit = false;

           if(uop_goes_to_commit)
           {
              /* we reach this only if pre_commit_available() */
              // add to commit buffer
              if((!uop->decode.in_fusion) || uop->decode.is_fusion_head)
                 core->commit->pre_commit_insert(uop);
              else
                 core->commit->pre_commit_fused_insert(uop);

              FU->occupancy--;
              zesto_assert(FU->occupancy >= 0,(void)0);
              FU->pipe[stage].uop = NULL;
           }
           else
           {
             FU->occupancy--;
             zesto_assert(FU->occupancy >= 0,(void)0);
             FU->pipe[stage].uop = NULL;
           }
         }
      } /* if not squashed */
      }
    }
  }


  /* shuffle the other stages forward (and update timing if we are in an exec_stal)*/
  for(i=0;i<knobs->exec.num_exec_ports;i++)
  {
    for(int j=0;j<port[i].num_FU_types;j++)
    {
       enum md_fu_class FU_type = port[i].FU_types[j];
       struct ALU_t * FU = port[i].FU[FU_type];
        if(FU->occupancy > 0)
        {
          /* If we are stalling, update timing, so that dependant instructions don't issue to execution */
          if(exec_stall)
          {
            for(int stage=FU->latency-1; stage > -1; stage--)
              if(FU->pipe[stage].uop)
              {
                 FU->pipe[stage].uop->timing.when_otag_ready++;

                 /* tag broadcast to dependents */
                 struct odep_t * odep = FU->pipe[stage].uop->exec.odep_uop;
                 while(odep)
                 {
                    int j;
                    tick_t when_ready = 0;
                    odep->uop->timing.when_itag_ready[odep->op_num] = FU->pipe[stage].uop->timing.when_otag_ready;

                    for(j=0;j<MAX_IDEPS;j++)
                    {
                       if(when_ready < odep->uop->timing.when_itag_ready[j])
                          when_ready = odep->uop->timing.when_itag_ready[j];
                    }
                    odep->uop->timing.when_ready = when_ready;

                    odep = odep->next;
                 }
              }
          }
          else
          {
            for(int stage=FU->latency-1;stage > 0; stage--)
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


//process pipe stages before actual FUs - payload pipe
//relies that we have a fixed 3-stage pipe
//1st stage - address generation
//2nd - cache access
//3rd - cache access res + leave to FU
//FIXME: This should be configurable

  zesto_assert(knobs->exec.payload_depth == 3, (void)0);
  int stage = knobs->exec.payload_depth-1;
  bool stall = false;
  struct uop_t * uop;

  for(i=0;i<knobs->exec.num_exec_ports;i++)
  {
   stall = port[i].when_stalled != 0;
   if(port[i].occupancy > 0)
    {
      uop = port[i].payload_pipe[stage].uop;
      work_found = true;

      /* uops leaving this pipe and going to the FUs */
      if (uop && port[i].payload_pipe[stage].action_id == uop->exec.action_id
                && !stall) /* uop is valid and hasn't been squashed */

        {
           int j;

           //loads should have already finished by now, if not, stall
           if(uop->decode.is_load)
           {
             if(!uop->exec.ovalue_valid)
             {
                stall = true;
                uop->exec.num_replays++;
                if(port[i].when_stalled == 0)
                  port[i].when_stalled = core->sim_cycle;
                continue;
             }
           }

//SK - we deal with fused uops on the same cycle. This assumes we are fusing LOAD, OP, STA, STD at most since in the IO pipe there are dedicated cycles for LOAD, STA and STD.
          if(uop->decode.in_fusion && uop->decode.is_load)
          {
             /* here we change the uop pointer to check for dependecies of the actual operation in the LOAD-OP-ST fused op; should be ok since we've already checked the LOAD and will care for the ST as early as in commit */
             zesto_assert(uop->exec.ovalue_valid, (void)0);

             uop = uop->decode.fusion_next;
             port[i].payload_pipe[stage].uop = uop;
             port[i].payload_pipe[stage].action_id = uop->exec.action_id;

             zesto_assert(uop, (void)0);
             zesto_assert(!uop->decode.is_load, (void)0);
             zesto_assert(!uop->decode.is_sta, (void)0);
             zesto_assert(!uop->decode.is_std, (void)0);
          }

          enum md_fu_class FU_class = uop->decode.FU_class;


          /* at this point a LOAD means non-fused LOAD, already executed -> go to commit
             stores here are also ready to go to pre-commit (they execute there). The uop is a fused STA-STD or non-fused, not part of larger fusion
             traps and NOPs also skip the execution units and go straight to pre_commit
             same happens for AGEN (generated mostly by LEA) */

         if(uop->decode.is_load || uop->decode.is_sta
               || uop->decode.is_nop || uop->Mop->decode.is_trap
               || (uop->decode.opflags & F_AGEN) == F_AGEN)
          {
//             if(!core->commit->pre_commit_available() || !can_issue_IO(uop))
//             {
               //STALL
//               ZESTO_STAT(core->stat.exec_uops_replayed++;)
//               uop->exec.num_replays++;
//               stall = true;
//               if(port[i].when_stalled == 0)
//                 port[i].when_stalled = core->sim_cycle;
//               continue;
//             }
//             else
//             {
                /* update occupancy */
//                port[i].payload_pipe[stage].uop = NULL;
//                port[i].occupancy--;
//                zesto_assert(port[i].occupancy >= 0, (void)0);

//                if(uop->decode.is_nop || uop->Mop->decode.is_trap)
//                {
//                  uop->timing.when_exec = core->sim_cycle;
//                  uop->timing.when_issued = core->sim_cycle;
//                  uop->timing.when_completed = core->sim_cycle;
//                }

//                core->commit->pre_commit_insert(uop);
                /* loads can be safely removed from load queue */
//                if(uop->decode.is_load)
//                {
//                   zesto_assert(uop->exec.ovalue_valid, (void)0);
//                   LDQ_deallocate(uop);
//                }
//             }
          }
          else
          {
             for(j=0;j<MAX_IDEPS;j++)
                if (uop->timing.when_ival_ready[j] <= core->sim_cycle)
                   uop->exec.ivalue_valid[j] = true;

             int all_ready = true;
             for(j=0;j<MAX_IDEPS;j++)
                all_ready &= uop->exec.ivalue_valid[j];

             /* have all input values arrived and FU available */
             if((!all_ready) || (port[i].FU[FU_class]->pipe[0].uop != NULL) || (port[i].FU[FU_class]->when_executable > core->sim_cycle) || !can_issue_IO(uop))
             {
               /* no, stall */
//SK - code for putting instruction back to sleep removed
               if (uop->timing.when_ready <= core->sim_cycle) /* supposed to be ready in the past */
               {
                 uop->timing.when_ready = core->sim_cycle+1;
                 /* ignore OO tornado breaker here, we stall */
               }
#ifdef ZTRACE
               ztrace_print(uop, "e|stall|uop cannot go to ALU %d %lld", uop->exec.num_replays,uop->timing.when_ready);
#endif
              /* stall*/
               ZESTO_STAT(core->stat.exec_uops_replayed++;)
               uop->exec.num_replays++;
               stall = true;
               if(port[i].when_stalled == 0)
                  port[i].when_stalled = core->sim_cycle;
               //continue;

             }
             else /* uop ready to leave to FU */
             {
#ifdef ZTRACE
                ztrace_print(uop,"e|payload|uop goes to ALU");
#endif
                ZESTO_STAT(core->stat.exec_uops_issued++;)

                if(!uop->decode.is_load)
                {
                   int fp_penalty = ((REG_IS_IN_FP_UNIT(uop->decode.odep_name) && !(uop->decode.opflags & F_FCOMP)) ||
                                    (!REG_IS_IN_FP_UNIT(uop->decode.odep_name) && (uop->decode.opflags & F_FCOMP)))?knobs->exec.fp_penalty:0;
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

                   odep = odep->next;
                }

                //SK - removed code handling RS deallocation

                uop->timing.when_exec = core->sim_cycle;

                /* this port has the proper FU and the first stage is free */
                zesto_assert((port[i].FU[FU_class] && (port[i].FU[FU_class]->pipe[0].uop == NULL)), (void)0);

                port[i].FU[FU_class]->pipe[0].uop = uop;
                port[i].FU[FU_class]->pipe[0].action_id = uop->exec.action_id;
                port[i].FU[FU_class]->occupancy++;
                port[i].FU[FU_class]->when_executable = core->sim_cycle + port[i].FU[FU_class]->issue_rate;
                check_for_work = true;


                /* in a fusion, deallocate a previous LD from the the LDQ here (done here to keep program order in LDQ deallocation) */
                if(uop->decode.in_fusion)
                {
                  struct uop_t * curr_uop = uop->decode.fusion_head;
                  while(curr_uop && curr_uop != uop)
                  {
                    if(curr_uop->decode.is_load)
                    {
                      zesto_assert(curr_uop->alloc.LDQ_index != -1, (void)0);
                      LDQ_deallocate(curr_uop);
                      break;
                    }
                    curr_uop = curr_uop->decode.fusion_next;
                  }
                }

                /* deal with OP-x-OP fusion */
                bool valid_fusion_next = false;

                if(uop->decode.in_fusion && uop->decode.fusion_next && !uop->decode.fusion_next->decode.is_sta)
                   valid_fusion_next = true;


                if(!valid_fusion_next)
                {
                  /* update occupancy */
                  port[i].payload_pipe[stage].uop = NULL;
                  port[i].occupancy--;
                  zesto_assert(port[i].occupancy >= 0, (void)0);
                }
                /* This can happen when we are fusing the OP-x-OP format.
                   In that case, simply keep the next part of the fusion at the end of issue*/
                else
                {
                  port[i].payload_pipe[stage].uop = uop->decode.fusion_next;
                  port[i].payload_pipe[stage].action_id = uop->decode.fusion_next->exec.action_id;

                  /* don't shuffle pipe since we aren't vacating a stage */
                  stall = true;
                  if(port[i].when_stalled == 0)
                     port[i].when_stalled = core->sim_cycle;
                  //continue;
               }


             }
          }
        }
        else if(uop && (port[i].payload_pipe[stage].action_id != uop->exec.action_id)
                    && !stall) /* uop has been squashed */
        {
#ifdef ZTRACE
          ztrace_print(uop,"e|payload|on exit from payload, uop discovered to have been squashed");
#endif

          /* in a fusion, deallocate a previous LD from the the LDQ here (done here to keep program order in LDQ deallocation) */
          if(uop->decode.in_fusion)
          {
            struct uop_t * curr_uop = uop->decode.fusion_head;
            while(curr_uop && curr_uop != uop)
            {
              if(curr_uop->decode.is_load)
              {
                zesto_assert(curr_uop->alloc.LDQ_index != -1, (void)0);
                LDQ_deallocate(curr_uop);
                break;
              }
              curr_uop = curr_uop->decode.fusion_next;
            }
          }

          if(uop->decode.is_load && uop->alloc.LDQ_index != -1)
            LDQ_deallocate(uop);


          port[i].payload_pipe[stage].uop = NULL;
          port[i].occupancy--;
          zesto_assert(port[i].occupancy >= 0, (void)0);
        }
        else if(!uop)/* no uops in last stage, clear stall flag if pipe stalled */
        {
           stall = false;
           port[i].when_stalled = 0;
        }

    }
  }


  for(i=0;i<knobs->exec.num_exec_ports;i++)
  {
      stall = port[i].when_stalled != 0;
      stage = knobs->exec.payload_depth - 2;
      uop = port[i].payload_pipe[stage].uop;

      /* uop in mem DC1 stage */
      /* LDQ insertion moved to end of alloc; AGEN in previous stage validates load - nothing to do here */

      /* if IO pipe stalled, just generate stats */
      if(stall
         && port[i].payload_pipe[stage+1].uop != NULL)
      /* The stall may be generated by a younger instruction waiting on us to
         issue in order. If this is the case, next stage wll be empty and we
         are safe to go there in order to avoid deadlock. */
      {
#ifdef ZTRACE
         ztrace_print(uop,"e|payload|payload stalled, no shuffling");
#endif
         ZESTO_STAT(core->stat.exec_uops_replayed++;)
         if(uop) uop->exec.num_replays++;
      }
      else
      /* shuffle uop to next stage */
      {
         port[i].payload_pipe[stage+1] = port[i].payload_pipe[stage];
         port[i].payload_pipe[stage].uop = NULL;
      }


      stage--;
      zesto_assert(stage >= 0, (void)0);
      uop = port[i].payload_pipe[stage].uop;

      /* uop in mem AGEN stage */
      if(uop)
      {
         struct uop_t * curr_uop = uop;
         while(curr_uop)
         {
           if(curr_uop->decode.is_load && !stall)
           {
             zesto_assert(curr_uop->alloc.LDQ_index != -1, (void)0);

//             bool load_ready = true;
//             for(int j=0;j<MAX_IDEPS;j++)
//               load_ready &= curr_uop->exec.ivalue_valid[j];

//             if(!load_ready)
//             {
//               stall = true;
//               if(port[i].when_stalled == 0)
//                 port[i].when_stalled = core->sim_cycle;

//               ZESTO_STAT(core->stat.exec_uops_replayed++;)
//               uop->exec.num_replays++;
//             }
//             else
//             {
               /* this validates the AGEN - just use the oracle value */
               LDQ[curr_uop->alloc.LDQ_index].virt_addr = curr_uop->oracle.virt_addr;
               LDQ[curr_uop->alloc.LDQ_index].addr_valid = true;
               curr_uop->timing.when_exec = core->sim_cycle;
               /* actual load processing in LDQ_schedule() */
//             }

             break;
           }

           /* LEA workaround - execution of LEA instruction on the Atom happens in the AGU, not in the ALU (see Intel Optimization manual) */
           if((curr_uop->decode.opflags & F_AGEN) == F_AGEN && !stall)
           {
             for(int j=0;j<MAX_IDEPS;j++)
                if (curr_uop->timing.when_ival_ready[j] <= core->sim_cycle)
                   curr_uop->exec.ivalue_valid[j] = true;

             bool agen_ready = true;
             for(int j=0;j<MAX_IDEPS;j++)
                agen_ready &= curr_uop->exec.ivalue_valid[j];


             if(!agen_ready)
             {
               stall = true;
               if(port[i].when_stalled == 0)
                 port[i].when_stalled = core->sim_cycle;

               ZESTO_STAT(core->stat.exec_uops_replayed++;)
               uop->exec.num_replays++;

               break;
             }


             curr_uop->exec.ovalue_valid = true;
             curr_uop->exec.ovalue = uop->oracle.ovalue;

             curr_uop->timing.when_exec = core->sim_cycle;
             curr_uop->timing.when_completed = core->sim_cycle;

             /* bypass output value to dependents */
             struct odep_t * odep = curr_uop->exec.odep_uop;
             while(odep)
             {
               zesto_assert(!odep->uop->exec.ivalue_valid[odep->op_num], (void)0);
               odep->uop->exec.ivalue_valid[odep->op_num] = true;
               if(odep->aflags)
                 odep->uop->exec.ivalue[odep->op_num].dw = curr_uop->exec.oflags;
               else
                 odep->uop->exec.ivalue[odep->op_num] = curr_uop->exec.ovalue;

               odep->uop->timing.when_ival_ready[odep->op_num] = core->sim_cycle;
               odep = odep->next;
             }
             break;
           }

           if(curr_uop->decode.in_fusion)
             curr_uop = curr_uop->decode.fusion_next;
           else
             curr_uop = NULL;
         }

         /* if stalling, just update stats */
         if(stall)
         {
           ZESTO_STAT(core->stat.exec_uops_replayed++;)
           uop->exec.num_replays++;
         }
         /* shuffle uop to next stage */
         else
         {
           port[i].payload_pipe[stage+1] = port[i].payload_pipe[stage];
           port[i].payload_pipe[stage].uop = NULL;
         }
      }


      if(!stall)
        port[i].when_stalled = 0;
  }
  check_for_work = work_found;
}

/* not entirely sure what the architectural counterpart of this is(scoreboard?), but we use it in the simulator to check if issuing won't break the program order (after issue no reordering can be done) */
bool core_exec_IO_DPM_t::can_issue_IO(struct uop_t * const uop)
{
  struct core_knobs_t * knobs = core->knobs;

  /* Assuming we issue now, when will the value be ready - used to check if we don't break execution order if we issue */
  tick_t when_otag_ready;
  if(!uop->decode.is_load && !uop->decode.is_sta && !uop->decode.is_std
     && !uop->decode.is_nop && !uop->Mop->decode.is_trap
     && (uop->decode.opflags & F_AGEN) != F_AGEN)
  {
    int fp_penalty = ((REG_IS_IN_FP_UNIT(uop->decode.odep_name) && !(uop->decode.opflags & F_FCOMP)) ||
                     (!REG_IS_IN_FP_UNIT(uop->decode.odep_name) && (uop->decode.opflags & F_FCOMP)))?knobs->exec.fp_penalty:0;
     when_otag_ready = core->sim_cycle + port[uop->alloc.port_assignment].FU[uop->decode.FU_class]->latency + fp_penalty;
  }
  else /* loads and stores go directly to pre_commit, as well as traps and nops */
     when_otag_ready = core->sim_cycle;


  for(int i=0;i<knobs->exec.num_exec_ports;i++)
  {
     for(int j=0;j<port[i].num_FU_types;j++)
     {
       enum md_fu_class FU_type = port[i].FU_types[j];
       struct ALU_t * FU = port[i].FU[FU_type];
       if(FU && (FU->occupancy > 0))
       {
         int stage = FU->latency-1;
         for(;stage>=0;stage--)
         {
            struct uop_t * curr_uop = FU->pipe[stage].uop;
            if(curr_uop)
            {
              if(curr_uop == uop)
                continue;


              /* issued uop prior in program order */
               if(uop->decode.uop_seq > curr_uop->decode.uop_seq)
               {
/* when_otag_rady should be already assigned for instructions in exec */
                 if(when_otag_ready < curr_uop->timing.when_otag_ready)
                    return false;
               }

            }
         }
       }
     }

     /* Also check last stage of payload (issue) pipe - there may be a prior op there (especially a stalled LD waiting for cache) */

     int stage = knobs->exec.payload_depth-1;
     struct uop_t * curr_uop = port[i].payload_pipe[stage].uop;
     if(curr_uop)
     {
        if(curr_uop == uop)
          continue;

        /* only fusion head occupies space in issue pipe */
        if(uop->decode.in_fusion && uop->decode.fusion_head == curr_uop)
          continue;

        /* issued uop prior in program order */
        if(uop->decode.uop_seq > curr_uop->decode.uop_seq)
        {
/* when_completed should be already assigned (and reliable since only loads can have variable latencies, but they are already processed) */

             tick_t when_curr_ready;
             int curr_port = curr_uop->alloc.port_assignment;
             enum md_fu_class curr_FU_class = curr_uop->decode.FU_class;

             if(curr_uop->decode.is_load)
                when_curr_ready = curr_uop->timing.when_completed;
             else if(curr_uop->decode.is_sta || curr_uop->decode.is_std
                     || curr_uop->decode.is_nop || curr_uop->Mop->decode.is_trap
                     || (curr_uop->decode.opflags & F_AGEN) == F_AGEN)
                when_curr_ready = TICK_T_MAX;
             else
             {
                 /* if curr_uop doesn't have all operands ready, it can't issue at this cycle */
                 bool operands_ready = true;
                 for(int ind = 0; ind < MAX_IDEPS; ind++)
                    operands_ready &= curr_uop->exec.ivalue_valid[ind];

                 if(!operands_ready)
                    when_curr_ready = TICK_T_MAX;
                 else
                 {
                    int curr_fp_penalty = ((REG_IS_IN_FP_UNIT(curr_uop->decode.odep_name) && !(curr_uop->decode.opflags & F_FCOMP)) ||
                                          (!REG_IS_IN_FP_UNIT(curr_uop->decode.odep_name) && (curr_uop->decode.opflags & F_FCOMP)))?knobs->exec.fp_penalty:0;
                    when_curr_ready = core->sim_cycle + port[curr_port].FU[curr_FU_class]->latency + curr_fp_penalty;
                 }

                 //If curr_uop is all set, but the FU it is supposed to go to is full or busy, it won't get issued this cycle and we should play safe
                 if((port[curr_port].FU[curr_FU_class]->pipe[0].uop != NULL) || (port[curr_port].FU[curr_FU_class]->when_executable > core->sim_cycle))
                    when_curr_ready = TICK_T_MAX;
             }

             if(when_otag_ready < when_curr_ready)
              return false;

             if(curr_uop->decode.in_fusion)
             {
               while(curr_uop &&
                     !curr_uop->decode.is_sta)
               {
                  if(when_otag_ready < curr_uop->timing.when_otag_ready)
                    return false;

                  curr_uop = curr_uop->decode.fusion_next;
               }
             }
        }

     }
     /* uops prior in program order can be in the prevoius stages of issue pipe. They surely need at least one cycle to go to the last issued stage before going to exec. So, don't allow issue */
     for(stage--;stage>=0;stage--)
     {
        curr_uop = port[i].payload_pipe[stage].uop;
        if(curr_uop)
          if(curr_uop->decode.uop_seq < uop->decode.uop_seq)
             return false;
     }

  }


  return true;
}

/* called by commit - tries to schedule a store for single cycle execution */
/* Assumes writeback cache - no waiting, just mark as ready and leave the cache subsystem handle the rest */
bool core_exec_IO_DPM_t::exec_fused_ST(struct uop_t * const uop)
{
  struct uop_t * curr_uop = uop;
  struct core_knobs_t * knobs = core->knobs;

  zesto_assert(curr_uop->decode.is_sta, false);

  if(!STQ_available())
     return false;

  //No separate execution units for stores (directly issued to pre_commit), so need to set stats (move to issue?)
  curr_uop->timing.when_exec = core->sim_cycle;
  curr_uop->timing.when_completed = core->sim_cycle;
  core->exec->STQ_insert_sta(curr_uop);

  zesto_assert((curr_uop->alloc.STQ_index >= 0) && (curr_uop->alloc.STQ_index < knobs->exec.STQ_size), false);
  zesto_assert(!STQ[curr_uop->alloc.STQ_index].addr_valid, false);
  STQ[curr_uop->alloc.STQ_index].virt_addr = curr_uop->oracle.virt_addr;
  STQ[curr_uop->alloc.STQ_index].addr_valid = true;

  /* In a rare event, another uop (other than the STD) may depend on
     the result of the STA. Since, presumably, store address calculation
     is done here, we need to update STA dependants */
  struct odep_t * odep = curr_uop->exec.odep_uop;
  while(odep)
  {
     zesto_assert(!odep->uop->exec.ivalue_valid[odep->op_num], false);
     odep->uop->exec.ivalue_valid[odep->op_num] = true;
     if(odep->aflags)
        odep->uop->exec.ivalue[odep->op_num].dw = curr_uop->exec.oflags;
     else
        odep->uop->exec.ivalue[odep->op_num] = curr_uop->exec.ovalue;
     odep->uop->timing.when_ival_ready[odep->op_num] = core->sim_cycle;
     odep = odep->next;
  }


  /* move on to STD */
  zesto_assert(curr_uop->decode.in_fusion, false);
  curr_uop = curr_uop->decode.fusion_next;
  zesto_assert(curr_uop, false);
  zesto_assert(curr_uop->decode.is_std, false);

  curr_uop->timing.when_exec = core->sim_cycle;
  curr_uop->timing.when_completed = core->sim_cycle;
  core->exec->STQ_insert_std(curr_uop);

  zesto_assert((curr_uop->alloc.STQ_index >= 0) && (curr_uop->alloc.STQ_index < knobs->exec.STQ_size), false);
  zesto_assert(!STQ[curr_uop->alloc.STQ_index].value_valid, false);

  curr_uop->exec.ovalue_valid = true;
  curr_uop->exec.ovalue = curr_uop->oracle.ovalue;

  STQ[curr_uop->alloc.STQ_index].value = curr_uop->exec.ovalue;
  STQ[curr_uop->alloc.STQ_index].value_valid = true;


  return true;
}

#endif
