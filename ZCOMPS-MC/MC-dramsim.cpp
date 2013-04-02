/* MC-dramsim.cpp: DRAMSim2 memory controller */
/*
 * __COPYRIGHT__ KB
 */

#include <iostream>
#include <fstream>
#include <list>
#include <stdint.h>
#include <../DRAMSim2/DRAMSim.h>

#ifdef MC_PARSE_ARGS
if(!strcasecmp("dramsim",type))
{
  string config_path;
  string part_name;

  char buf_config[100];
  char buf_part[100];

  // Format specifiers are readable and easily maintainable
  if(sscanf(opt_string,"%*[^:]:%[^:]:%s", buf_config, buf_part) != 2)
    fatal("bad memory controller options string %s (should be \"dramsim:config_path:part_name\")",opt_string);

  config_path = buf_config;
  part_name = buf_part;

  return new MC_dramsim_t(config_path, part_name);
}
#else

using namespace DRAMSim;

class MC_dramsim_t:public MC_t
{
  private:
    MultiChannelMemorySystem* mem;
    std::list<MC_action_t> outstanding_reqs;

    bool finished_init;

    double memPeriodNs;
    double cpuPeriodNs;
    double accumulatedNs;
    double nextMemUpdate;

    string config_path;
    string part_name;

    int mem_cycle;

  public:

  /* Callback from DRAMSim, we use the addr to check the outstanding request list
   */
  void mem_complete(unsigned id, uint64_t addr, uint64_t cycle)
  {
    // Find first outstanding request for that addr
    std::list<MC_action_t>::iterator it;
    bool found = false;
    for(it = outstanding_reqs.begin(); it != outstanding_reqs.end(); it++) {
      if((*it).addr == addr && (*it).when_finished == TICK_T_MAX) {
        (*it).when_finished = sim_cycle;
        total_dram_cycles += (*it).when_finished - (*it).when_started;
        found = true;
        break;
      }
    }
    assert(found);
  }

  MC_dramsim_t(string dramsim_root_path, string dramsim_part_name)
  {
    init();

    config_path = dramsim_root_path;
    part_name = dramsim_part_name;

    // Parse memory clock period from system file
    // It is stored as ns
    std::ifstream fin;
    string part_file_name = config_path + "/" + part_name;
    string line;
    bool foundMemClock = false;
    fin.open(part_file_name.c_str());
    assert(fin.is_open());
    while(getline(fin, line)) {
      if(line.find("tCK=") != string::npos) {
        sscanf(line.c_str(), "%*[^=]=%lf", &memPeriodNs);
        foundMemClock = true;
        break;
      }
    }
    assert(foundMemClock);
    fin.close();
  }

  ~MC_dramsim_t()
  {
    delete mem;
  }

  void finish_init()
  {
    finished_init = true;
    accumulatedNs = 0;

    nextMemUpdate = memPeriodNs;
    cpuPeriodNs = 1.0 / (uncore->cpu_speed / (1e3));
    assert(memPeriodNs > cpuPeriodNs);
    std::cerr << "Memperiod is about ns: " << memPeriodNs << std::endl;

    // DRAMSim callbacks
    TransactionCompleteCB *read_cb = new Callback<MC_dramsim_t, void, unsigned, uint64_t, uint64_t>(this, &MC_dramsim_t::mem_complete);
    TransactionCompleteCB *write_cb = new Callback<MC_dramsim_t, void, unsigned, uint64_t, uint64_t>(this, &MC_dramsim_t::mem_complete);

    mem = getMemorySystemInstance(part_name,
                  "system.ini",
                  config_path,
                  "unused",
                  4096); // 4 gigs of DRAM
    //DDR3_micron_16M_8B_x8_sg15.ini
    mem->RegisterCallbacks(read_cb, write_cb, NULL);

    uint64_t cpuClkFreqHz = (uint64_t)(uncore->cpu_speed * (1e6));
    mem->setCPUClockSpeed(cpuClkFreqHz);
  }

  MC_ENQUEUABLE_HEADER
  {
    assert(finished_init);
    // Need the addr paramater to check the proper MC/channel
    return mem->willAcceptTransaction((uint64_t)addr);
  }

  /* Enqueue a memory command (read/write) to the memory controller. */
  MC_ENQUEUE_HEADER
  {
    assert(finished_init);
    MC_assert(mem->willAcceptTransaction((uint64_t)addr),(void)0);

    outstanding_reqs.push_back(MC_action_t());
    MC_action_t* req = &(outstanding_reqs.back());

    // I kept the same structure for the request as in the other
    // ZCOMPS-MC, to be consistent.
    MC_assert(req->valid == false,(void)0);
    req->valid = true;
    req->prev_cp = prev_cp;
    req->cmd = cmd;
    req->addr = addr;
    req->linesize = linesize;
    req->op = op;
    req->action_id = action_id;
    req->MSHR_bank = MSHR_bank;
    req->MSHR_index = MSHR_index;
    req->cb = cb;
    req->get_action_id = get_action_id;
    req->when_enqueued = sim_cycle;
    req->when_started = sim_cycle;
    req->when_finished = TICK_T_MAX;
    req->when_returned = TICK_T_MAX;

    assert(req->addr == (uint64_t)req->addr);

    mem->addTransaction(req->cmd == CACHE_WRITE, req->addr);

    total_accesses++;
  }

  /* This is called each cycle to process the requests in the memory controller queue. */
  MC_STEP_HEADER
  {
    // Need to init here, since in constructor, uncore pointer is not yet valid
    if(!finished_init) {
      finish_init();
      finished_init = true;
    }

    // See if it's time to run a memory tick
    accumulatedNs += cpuPeriodNs;
    if(accumulatedNs < nextMemUpdate) {
      return;
    }

    // Schedule next memory tick
    nextMemUpdate += memPeriodNs;

    // Tick the memory
    mem->update();

    /* walk request queue and process requests that have completed. */
    std::list<MC_action_t>::iterator it;
    for(it = outstanding_reqs.begin(); it != outstanding_reqs.end(); ) {

      MC_action_t* req = &(*it);

      if((req->when_finished <= sim_cycle) && (req->when_returned == TICK_T_MAX) && (!req->prev_cp || bus_free(uncore->fsb))) {

        req->when_returned = sim_cycle;
        total_service_cycles += sim_cycle - req->when_enqueued;

        /* fill previous level as appropriate */
        if(req->prev_cp) {
          fill_arrived(req->prev_cp,req->MSHR_bank,req->MSHR_index);
          bus_use(uncore->fsb,req->linesize>>uncore->fsb_DDR,req->cmd==CACHE_PREFETCH);
          it = outstanding_reqs.erase(it);
          break; /* might as well break, since only one request can writeback per cycle */
        }
        else { // is a write to dram (I think)
          it = outstanding_reqs.erase(it);
        }
      }
      else {
        it++;
      }
    }

  }

  MC_PRINT_HEADER
  {
    fprintf(stderr,"<<<<< MC >>>>>\n");
    std::list<MC_action_t>::iterator it;
    int i = 0;
    for(it = outstanding_reqs.begin(); it != outstanding_reqs.end(); it++, i++) {
      fprintf(stderr,"MC[%d]: ",i);
      if((*it).op)
        fprintf(stderr,"%p(%lld)",(*it).op,((struct uop_t*)((*it).op))->decode.uop_seq);
      fprintf(stderr," --> %s",(*it).prev_cp->name);
      fprintf(stderr," MSHR[%d][%d]",(*it).MSHR_bank,(*it).MSHR_index);
    }
    fprintf(stderr,"\n");
  }

};


#endif /* MC_PARSE_ARGS */
