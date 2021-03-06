#include <cstddef>

#include "misc.h"
#include "sim.h"
#include "stats.h"

#include "zesto-power.h"
#include "zesto-dvfs.h"
#include "zesto-oracle.h"
#include "zesto-core.h"
#include "zesto-fetch.h"
#include "zesto-bpred.h"
#include "zesto-uncore.h"
#include "zesto-cache.h"

#include "third_party/mcpat/XML_Parse.h"
#include "third_party/mcpat/mcpat.h"

class ParseXML *XML = NULL; //Interface to McPAT

double uncore_rtp;
double *cores_rtp;

double uncore_leakage;
double *cores_leakage;

FILE *rtp_file = NULL;

bool private_l2 = false;

double core_power_t::default_vdd;

enum device_type_t { HP = 0, LSTP = 1, LOP = 2 };

void init_power(void)
{
  XML = new ParseXML();
  XML->initialize();


  /* Translate uncore params */
  XML->sys.number_of_cores = system_knobs.num_cores;
  XML->sys.number_of_L1Directories = 0;
  XML->sys.number_of_L2Directories = 0;
  XML->sys.number_of_NoCs = 0;
  XML->sys.number_of_dir_levels = 0;
  XML->sys.homogeneous_cores = 0;
  XML->sys.homogeneous_L1Directories = 0;
  XML->sys.homogeneous_L2Directories = 0;
  XML->sys.core_tech_node = 45;
  XML->sys.target_core_clockrate = (int)core_knobs.default_cpu_speed;
  XML->sys.temperature = 380; // K
  XML->sys.interconnect_projection_type = 0; // aggressive
  XML->sys.longer_channel_device = 1; // use when appropirate
  XML->sys.machine_bits = 64;
  XML->sys.virtual_address_width = 64;
  XML->sys.physical_address_width = 52;
  XML->sys.virtual_memory_page_size = 4096;
  XML->sys.vdd = 1.0;

  int num_l2 = 0;
  bool has_hp_core = false;
  for (int i=0; i<system_knobs.num_cores; i++)
  {
    // If any core has a private L2, we assume L3 is LLC
    if(cores[i]->memory.DL2)
    {
      private_l2 = true;
      num_l2++;
    }

    // If any core is not in-order, we assume high speed devices
    if(strcmp(cores[i]->knobs->model, "IO-DPM"))
      has_hp_core = true;
  } 

  XML->sys.device_type = has_hp_core ? HP : LOP;

  XML->sys.Private_L2 = private_l2;
  XML->sys.number_of_L2s = private_l2 ? num_l2 : 1;
  XML->sys.number_cache_levels = private_l2 ? 3 : 2;
  XML->sys.number_of_L3s = private_l2 ? 1 : 0;

  // LLC is L2
  if (uncore->LLC && !private_l2)
  {
    XML->sys.L2[0].L2_config[0] = uncore->LLC->sets * uncore->LLC->assoc * uncore->LLC->linesize;
    XML->sys.L2[0].L2_config[1] = uncore->LLC->linesize;
    XML->sys.L2[0].L2_config[2] = uncore->LLC->assoc;
    // McPAT seems too optimistic for area and leakeage of multi-banked caches.
    // We hardcode banking to 1, which produces LLC leakage closer to our
    // Atom 330 measurements.
    XML->sys.L2[0].L2_config[3] = 1;
    // uncore->LLC->latency is in core clock cycles.
    // McPAT docs say they also use core clocks, but sharedcache.cc:1130 begs
    // to differ (for both L2_config[4,5]). So, we convert to uncore cycles.
    // Throughput [4] seems to be a misnomer. [4] is the target array
    // cycle time. Arrays with *higher* times gets discarded.
    // We can think of the ratio [5] / [4] as the # pipeline stages of the cache.
    // Setting that ratio to 1 (we know [5]) would leave both pipelined and
    // non-pipelined designs on the table. >1 would mean we always want pipelining
    // and sometimes cacti has to try harder to give us that. <1 doesn't make sense.
    XML->sys.L2[0].L2_config[5] = uncore->LLC->latency * (uncore_knobs.LLC_speed / core_knobs.default_cpu_speed);
    XML->sys.L2[0].L2_config[4] = XML->sys.L2[0].L2_config[5];

    // McPAT doesn't use below two apparantly
    XML->sys.L2[0].L2_config[6] = uncore->LLC->linesize;
    XML->sys.L2[0].L2_config[7] = (uncore->LLC->write_policy == WRITE_THROUGH) ? 0 : 1;

    // # read ports
    XML->sys.L2[0].ports[0] = 1;
    // # write ports
    XML->sys.L2[0].ports[1] = 1;
    // # rw ports
    XML->sys.L2[0].ports[2] = 1;

    // # MSHRs
    XML->sys.L2[0].buffer_sizes[0] = uncore->LLC->MSHR_size;
    // # fill buffers
    XML->sys.L2[0].buffer_sizes[1] = uncore->LLC->heap_size;
    // # PF buffers
    XML->sys.L2[0].buffer_sizes[2] = uncore->LLC->PFF_size;
    // # WB buffers
    XML->sys.L2[0].buffer_sizes[3] = uncore->LLC->MSHR_WB_size;

    XML->sys.L2[0].clockrate = (int)uncore_knobs.LLC_speed;
    XML->sys.L2[0].device_type = LOP;

    XML->sys.L2[0].vdd = 0;
    XML->sys.L2[0].power_gating_vcc = -1;
  } else if (uncore->LLC) // LLC is L3
  {
    XML->sys.L3[0].L3_config[0] = uncore->LLC->sets * uncore->LLC->assoc * uncore->LLC->linesize;
    XML->sys.L3[0].L3_config[1] = uncore->LLC->linesize;
    XML->sys.L3[0].L3_config[2] = uncore->LLC->assoc;
    // McPAT doesn't seem to like heavily banked caches.
    // We might want to hardcode this to 1,2,4 for big multicores.
    XML->sys.L3[0].L3_config[3] = uncore->LLC->banks;
    // Same as L2 case above -- convert to uncore cycles and set throughput == latency.
    XML->sys.L3[0].L3_config[5] = uncore->LLC->latency * (uncore_knobs.LLC_speed / core_knobs.default_cpu_speed);
    XML->sys.L3[0].L3_config[4] = XML->sys.L3[0].L3_config[5];

    // # read ports
    XML->sys.L3[0].ports[0] = 1;
    // # write ports
    XML->sys.L3[0].ports[1] = 1;
    // # rw ports
    XML->sys.L3[0].ports[2] = 1;

    // # MSHRs
    XML->sys.L3[0].buffer_sizes[0] = uncore->LLC->MSHR_size;
    // # fill buffers
    XML->sys.L3[0].buffer_sizes[1] = uncore->LLC->heap_size;
    // # PF buffers
    XML->sys.L3[0].buffer_sizes[2] = uncore->LLC->PFF_size;
    // # WB buffers
    XML->sys.L3[0].buffer_sizes[3] = uncore->LLC->MSHR_WB_size;

    XML->sys.L3[0].clockrate = (int)uncore_knobs.LLC_speed;
    XML->sys.L3[0].device_type = LOP;
  }

  XML->sys.mc.number_mcs = 0;
  XML->sys.flashc.number_mcs = 0;
  XML->sys.niu.number_units = 0;
  XML->sys.pcie.number_units = 0;

  for (int i=0; i<system_knobs.num_cores; i++)
    cores[i]->power->translate_params(&XML->sys.core[i], &XML->sys.L2[i]);

  cores_leakage = (double*)calloc(system_knobs.num_cores, sizeof(*cores_leakage));
  if (cores_leakage == NULL)
    fatal("couldn't allocate memory");

  mcpat_initialize(XML, &cerr, cores_leakage, &uncore_leakage, 5);

  cores_rtp = (double*)calloc(system_knobs.num_cores, sizeof(*cores_rtp));
  if (cores_rtp == NULL)
    fatal("couldn't allocate memory");

  if (system_knobs.power.rtp_interval > 0)
  {
    assert(system_knobs.power.rtp_filename);
    rtp_file = fopen(system_knobs.power.rtp_filename, "w");
    if (rtp_file == NULL)
      fatal("couldn't open rtp power file: %s", system_knobs.power.rtp_filename);
  }

  // Initialize Vdd from mcpat defaults
  core_power_t::default_vdd = g_tp.peri_global.Vdd;
  for (int i=0; i<system_knobs.num_cores; i++)
    if (cores[i]->vf_controller) {
      cores[i]->vf_controller->vdd = g_tp.peri_global.Vdd;
      cores[i]->vf_controller->vf_controller_t::change_vf();
    }
}

void deinit_power(void)
{
  free(cores_rtp);
  if (rtp_file)
    fclose(rtp_file);
  mcpat_finalize();
}

void translate_uncore_stats(xiosim::stats::StatsDatabase* sdb, root_system* stats)
{
  xiosim::stats::Statistic<counter_t>* curr_stat = nullptr;

  if (uncore->LLC)
  {
    if (!private_l2)
    {
      curr_stat = stat_find_stat<counter_t>(sdb, "LLC.load_lookups");
      stats->L2[0].read_accesses = curr_stat->get_final_val();
      curr_stat = stat_find_stat<counter_t>(sdb, "LLC.load_misses");
      stats->L2[0].read_misses = curr_stat->get_final_val();
      curr_stat = stat_find_stat<counter_t>(sdb, "LLC.store_lookups");
      stats->L2[0].write_accesses = curr_stat->get_final_val();
      curr_stat = stat_find_stat<counter_t>(sdb, "LLC.store_misses");
      stats->L2[0].write_misses = curr_stat->get_final_val();
    }
    else {
      curr_stat = stat_find_stat<counter_t>(sdb, "LLC.load_lookups");
      stats->L3[0].read_accesses = curr_stat->get_final_val();
      curr_stat = stat_find_stat<counter_t>(sdb, "LLC.load_misses");
      stats->L3[0].read_misses = curr_stat->get_final_val();
      curr_stat = stat_find_stat<counter_t>(sdb, "LLC.store_lookups");
      stats->L3[0].write_accesses = curr_stat->get_final_val();
      curr_stat = stat_find_stat<counter_t>(sdb, "LLC.store_misses");
      stats->L3[0].write_misses = curr_stat->get_final_val();
    }
  }

  curr_stat = stat_find_stat<tick_t>(sdb, "uncore.sim_cycle");
  stats->total_cycles = curr_stat->get_final_val();
}

void compute_power(xiosim::stats::StatsDatabase* sdb, bool print_power)
{
  /* Get necessary simualtor stats */
  translate_uncore_stats(sdb, &XML->sys);
  for(int i=0; i<system_knobs.num_cores; i++)
    cores[i]->power->translate_stats(sdb, &XML->sys.core[i], &XML->sys.L2[i]);

  /* Invoke mcpat */
  mcpat_compute_energy(print_power, cores_rtp, &uncore_rtp);

  /* Print power trace */
  if (rtp_file)
  {
    for(int i=0; i<system_knobs.num_cores; i++) {
      /* Scale trace with voltage changes */
      double vdd = cores[i]->vf_controller->get_average_vdd();
      cores[i]->power->rt_power = cores_rtp[i] * (vdd * vdd) / (core_power_t::default_vdd * core_power_t::default_vdd);
      cores[i]->power->leakage_power = cores_leakage[i] * vdd / core_power_t::default_vdd;
      fprintf(rtp_file, "%.4f %.4f ", cores[i]->power->rt_power, cores[i]->power->leakage_power);
    }
    fprintf(rtp_file, "%.4f %.4f\n", uncore_rtp, uncore_leakage);
  }
}

void core_power_t::translate_params(system_core *core_params, system_L2 *L2_params)
{
  (void) L2_params;
  struct core_knobs_t *knobs = core->knobs;

  core_params->clock_rate = core->cpu_speed;
  core_params->opt_local = false;
  core_params->x86 = true;
  core_params->machine_bits = 64;
  core_params->virtual_address_width = 64;
  core_params->physical_address_width = 52; //XXX
  core_params->opcode_width = 16;
  core_params->micro_opcode_width = 8;
  core_params->instruction_length = 32;

  core_params->fetch_width = knobs->fetch.width;
  core_params->decode_width = knobs->decode.width;
  core_params->issue_width = knobs->exec.num_exec_ports;
  core_params->peak_issue_width = knobs->exec.num_exec_ports;
  core_params->commit_width = knobs->commit.width;

  core_params->ALU_per_core = knobs->exec.port_binding[FU_IEU].num_FUs;
  core_params->MUL_per_core = knobs->exec.port_binding[FU_IMUL].num_FUs;
  core_params->FPU_per_core = knobs->exec.port_binding[FU_FADD].num_FUs;
  core_params->instruction_buffer_size = knobs->fetch.IQ_size;
  core_params->decoded_stream_buffer_size = knobs->decode.uopQ_size;

  core_params->ROB_size = knobs->commit.ROB_size;
  core_params->load_buffer_size = knobs->exec.LDQ_size;
  core_params->store_buffer_size = knobs->exec.STQ_size;
  core_params->RAS_size = core->fetch->bpred->get_ras()->get_size();

  if (core->memory.DL1)
  {
    core_params->dcache.dcache_config[0] = core->memory.DL1->sets * core->memory.DL1->assoc * core->memory.DL1->linesize;
    core_params->dcache.dcache_config[1] = core->memory.DL1->linesize;
    core_params->dcache.dcache_config[2] = core->memory.DL1->assoc;
    core_params->dcache.dcache_config[3] = core->memory.DL1->banks;
    core_params->dcache.dcache_config[5] = core->memory.DL1->latency;
    // See LLC comment for setting throughput == latency.
    core_params->dcache.dcache_config[4] = core_params->dcache.dcache_config[5];
    core_params->dcache.dcache_config[6] = core->memory.DL1->linesize;
    core_params->dcache.dcache_config[7] = (core->memory.DL1->write_policy == WRITE_THROUGH) ? 0 : 1;


    // # MSHRs
    core_params->dcache.buffer_sizes[0] = core->memory.DL1->MSHR_size;
    // # fill buffers
    core_params->dcache.buffer_sizes[1] = core->memory.DL1->heap_size;
    // # PF buffers
    core_params->dcache.buffer_sizes[2] = core->memory.DL1->PFF_size;
    // # WB buffers
    core_params->dcache.buffer_sizes[3] = core->memory.DL1->MSHR_WB_size;
  }

  if (core->memory.IL1)
  {
    core_params->icache.icache_config[0] = core->memory.IL1->sets * core->memory.IL1->assoc * core->memory.IL1->linesize;
    core_params->icache.icache_config[1] = core->memory.IL1->linesize;
    core_params->icache.icache_config[2] = core->memory.IL1->assoc;
    core_params->icache.icache_config[3] = core->memory.IL1->banks;
    core_params->icache.icache_config[5] = core->memory.IL1->latency;
    // See LLC comment for setting throughput == latency.
    core_params->icache.icache_config[4] = core_params->icache.icache_config[5];
    core_params->icache.icache_config[6] = core->memory.IL1->linesize;
    core_params->icache.icache_config[7] = 1;

    core_params->icache.buffer_sizes[0] = 2;
    core_params->icache.buffer_sizes[1] = 2;
    core_params->icache.buffer_sizes[2] = 2;
    core_params->icache.buffer_sizes[3] = 2;
  }

  if (core->memory.ITLB)
  {
    core_params->itlb.number_entries = core->memory.ITLB->sets;
    core_params->itlb.cache_policy = 1;
  }

  if (core->memory.DTLB)
  {
    core_params->dtlb.number_entries = core->memory.DTLB->sets;
    core_params->dtlb.cache_policy = 1;
  }

  if (core->fetch->bpred)
  {
    class BTB_t *btb = core->fetch->bpred->get_dir_btb();
    if (btb)
    {
      core_params->BTB.BTB_config[0] = btb->get_num_entries();
      core_params->BTB.BTB_config[1] = btb->get_tag_width() / 8;
      core_params->BTB.BTB_config[2] = btb->get_num_ways();
      core_params->BTB.BTB_config[3] = 1; //# banks
      core_params->BTB.BTB_config[4] = 1; //troughput
      core_params->BTB.BTB_config[5] = 1; //latency
    }

    auto pred = core->fetch->bpred->get_dir_bpred()[0].get();
    core_params->predictor.local_predictor_entries  = pred->get_local_size();
    core_params->predictor.local_predictor_size[0]  = pred->get_local_width(0);
    core_params->predictor.local_predictor_size[1]  = pred->get_local_width(1);
    core_params->predictor.global_predictor_entries  = pred->get_global_size();
    core_params->predictor.global_predictor_bits  = pred->get_global_width();
    core_params->predictor.chooser_predictor_entries  = pred->get_chooser_size();
    core_params->predictor.chooser_predictor_bits  = pred->get_chooser_width();
  }

  // AF for max power computation
  core_params->IFU_duty_cycle = 1.0;
  core_params->LSU_duty_cycle = 0.5;
  core_params->MemManU_I_duty_cycle = 1.0;
  core_params->MemManU_D_duty_cycle = 0.5;
  core_params->ALU_duty_cycle = 1.0;
  core_params->MUL_duty_cycle = 0.3;
  core_params->FPU_duty_cycle = 0.3;
  core_params->ALU_cdb_duty_cycle = 1.0;
  core_params->MUL_cdb_duty_cycle = 0.3;
  core_params->FPU_cdb_duty_cycle = 0.3;
}

void core_power_t::translate_stats(xiosim::stats::StatsDatabase* sdb,
                                   system_core* core_stats,
                                   system_L2* L2_stats) {
  using namespace xiosim::stats;
  Statistic<counter_t>* stat = nullptr;
  Formula* formula = nullptr;

  int coreID = core->id;

  struct core_knobs_t* knobs = core->knobs;
  (void) L2_stats;
  //XXX: Ignore McPAT's DVFS calculation for now. We do our own scaling in
  // compute_power(). That is, until we can validate that McPAT is doing
  // more or less the right thing.
  core_stats->vdd = core_power_t::default_vdd;

  stat = stat_find_core_stat<counter_t>(sdb, coreID, "oracle_total_uops");
  core_stats->total_instructions = stat->get_final_val();

  stat = stat_find_core_stat<counter_t>(sdb, coreID, "oracle_total_branches");
  core_stats->branch_instructions = stat->get_final_val();
  stat = stat_find_core_stat<counter_t>(sdb, coreID, "num_jeclear");
  core_stats->branch_mispredictions = stat->get_final_val();
  stat = stat_find_core_stat<counter_t>(sdb, coreID, "oracle_total_loads");
  core_stats->load_instructions = stat->get_final_val();
  stat = stat_find_core_stat<counter_t>(sdb, coreID, "oracle_total_refs");
  core_stats->store_instructions = stat->get_final_val() - core_stats->load_instructions;
  formula = stat_find_core_formula(sdb, coreID, "oracle_num_uops");
  core_stats->committed_instructions = formula->evaluate();

  // core cycles at potentially variable frequency
  stat = stat_find_core_stat<tick_t>(sdb, coreID, "sim_cycle");
  core_stats->total_cycles = stat->get_final_val();

  // get average frequency for this period
  stat = stat_find_stat<tick_t>(sdb, "sim_cycle");
  core_stats->clock_rate = (int)ceil(core_stats->total_cycles * knobs->default_cpu_speed /
                                     (double)stat->get_final_val());

  core_stats->idle_cycles = 0;
  core_stats->busy_cycles = core_stats->total_cycles - core_stats->idle_cycles;

  stat = stat_find_core_stat<counter_t>(sdb, coreID, "regfile_reads");
  core_stats->int_regfile_reads = stat->get_final_val();
  stat = stat_find_core_stat<counter_t>(sdb, coreID, "fp_regfile_reads");
  core_stats->float_regfile_reads = stat->get_final_val();
  stat = stat_find_core_stat<counter_t>(sdb, coreID, "regfile_writes");
  core_stats->int_regfile_writes = stat->get_final_val();
  stat = stat_find_core_stat<counter_t>(sdb, coreID, "fp_regfile_writes");
  core_stats->float_regfile_writes = stat->get_final_val();

  stat = stat_find_core_stat<counter_t>(sdb, coreID, "oracle_total_calls");
  core_stats->function_calls = stat->get_final_val();

  stat = stat_find_core_stat<counter_t>(sdb, coreID, "int_FU_occupancy");
  core_stats->cdb_alu_accesses = stat->get_final_val();
  stat = stat_find_core_stat<counter_t>(sdb, coreID, "fp_FU_occupancy");
  core_stats->cdb_fpu_accesses = stat->get_final_val();
  stat = stat_find_core_stat<counter_t>(sdb, coreID, "mul_FU_occupancy");
  core_stats->cdb_mul_accesses = stat->get_final_val();
  core_stats->ialu_accesses = core_stats->cdb_alu_accesses;
  core_stats->fpu_accesses = core_stats->cdb_fpu_accesses;
  core_stats->mul_accesses = core_stats->cdb_mul_accesses;

  stat = stat_find_core_stat<counter_t>(sdb, coreID, "commit_insn");
  core_stats->pipeline_duty_cycle = (double) stat->get_final_val();
  stat = stat_find_core_stat<tick_t>(sdb, coreID, "sim_cycle");
  core_stats->pipeline_duty_cycle /= stat->get_final_val();
  core_stats->pipeline_duty_cycle /= knobs->commit.width;

  if (core->memory.ITLB)
  {
    stat = stat_find_core_stat<counter_t>(sdb, coreID, "ITLB.lookups");
    core_stats->itlb.total_accesses = stat->get_final_val();
    stat = stat_find_core_stat<counter_t>(sdb, coreID, "ITLB.misses");
    core_stats->itlb.total_misses = stat->get_final_val();
  }

  if (core->memory.DTLB)
  {
    stat = stat_find_core_stat<counter_t>(sdb, coreID, "DTLB.lookups");
    core_stats->dtlb.total_accesses = stat->get_final_val();
    stat = stat_find_core_stat<counter_t>(sdb, coreID, "DTLB.misses");
    core_stats->dtlb.total_misses = stat->get_final_val();
  }

  if (core->memory.IL1)
  {
    stat = stat_find_core_stat<counter_t>(sdb, coreID, "IL1.lookups");
    core_stats->icache.read_accesses = stat->get_final_val();
    stat = stat_find_core_stat<counter_t>(sdb, coreID, "IL1.misses");
    core_stats->icache.read_misses = stat->get_final_val();
  }

  if (core->memory.DL1)
  {
    stat = stat_find_core_stat<counter_t>(sdb, coreID, "DL1.load_lookups");
    core_stats->dcache.read_accesses = stat->get_final_val();
    stat = stat_find_core_stat<counter_t>(sdb, coreID, "DL1.load_misses");
    core_stats->dcache.read_misses = stat->get_final_val();
    stat = stat_find_core_stat<counter_t>(sdb, coreID, "DL1.store_lookups");
    core_stats->dcache.write_accesses = stat->get_final_val();
    stat = stat_find_core_stat<counter_t>(sdb, coreID, "DL1.store_misses");
    core_stats->dcache.write_misses = stat->get_final_val();
  }

  if (core->fetch->bpred->get_dir_btb())
  {
    stat = stat_find_core_stat<counter_t>(sdb, coreID, "BTB.lookups");
    core_stats->BTB.read_accesses = stat->get_final_val();
    stat = stat_find_core_stat<counter_t>(sdb, coreID, "BTB.updates");
    core_stats->BTB.write_accesses = stat->get_final_val();
    stat = stat_find_core_stat<counter_t>(sdb, coreID, "BTB.spec_updates");
    core_stats->BTB.write_accesses += stat->get_final_val();
  }
}

/* load in all definitions */
#include "xiosim/ZCORE-power.list.h"

/* default constructor */
core_power_t::core_power_t(struct core_t * _core):
  rt_power(0.0), leakage_power(0.0),
  core(_core)
{
}

std::unique_ptr<class core_power_t> power_create(const char * power_opt_string, struct core_t * core)
{
#define ZESTO_PARSE_ARGS
#include "xiosim/ZCORE-power.list.h"

  fatal("unknown power model type \"%s\"", power_opt_string);
#undef ZESTO_PARSE_ARGS
}
