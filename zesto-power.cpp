#include "thread.h"

#include "stats.h"
#include "zesto-power.h"
#include "zesto-oracle.h"
#include "zesto-core.h"
#include "zesto-fetch.h"
#include "zesto-bpred.h"
#include "zesto-uncore.h"

#include "XML_Parse.h"
#include "mcpat.h"

class ParseXML *XML = NULL; //Interface to McPAT

extern tick_t sim_cycle;
extern int num_threads;

double uncore_rtp;
double *cores_rtp;
FILE *rtp_file = NULL;

bool private_l2 = false;

void init_power(void)
{
  XML = new ParseXML();
  XML->initialize();


  /* Translate uncore params */
  XML->sys.number_of_cores = num_threads;
  XML->sys.number_of_L1Directories = 0;
  XML->sys.number_of_L2Directories = 0;
  XML->sys.number_of_NoCs = 0;
  XML->sys.number_of_dir_levels = 0;
  XML->sys.homogeneous_cores = 0;
  XML->sys.homogeneous_L1Directories = 0;
  XML->sys.homogeneous_L2Directories = 0;
  XML->sys.core_tech_node = 45;
  XML->sys.target_core_clockrate = (int)uncore->cpu_speed;
  XML->sys.temperature = 380; // K
  XML->sys.interconnect_projection_type = 0; // aggressive
  XML->sys.longer_channel_device = 1; // use when appropirate
  XML->sys.machine_bits = 64;
  XML->sys.virtual_address_width = 64;
  XML->sys.physical_address_width = 52;
  XML->sys.virtual_memory_page_size = 4096;

  int num_l2 = 0;
  bool has_hp_core = false;
  for (int i=0; i<num_threads; i++)
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

  XML->sys.device_type = has_hp_core ? 0 /*HP*/: 2 /*LOP*/;

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
    XML->sys.L2[0].L2_config[3] = uncore->LLC->banks;
    XML->sys.L2[0].L2_config[4] = 1;
    XML->sys.L2[0].L2_config[5] = uncore->LLC->latency;
    XML->sys.L2[0].L2_config[6] = uncore->LLC->bank_width;
    XML->sys.L2[0].L2_config[7] = (uncore->LLC->write_policy == WRITE_THROUGH) ? 0 : 1;

    XML->sys.L2[0].ports[0] = 1;
    XML->sys.L2[0].ports[1] = 0;
    XML->sys.L2[0].ports[2] = 0;

    XML->sys.L2[0].buffer_sizes[0] = 1;
    XML->sys.L2[0].buffer_sizes[1] = 2;
    XML->sys.L2[0].buffer_sizes[2] = 2;
    XML->sys.L2[0].buffer_sizes[3] = 2;

    XML->sys.L2[0].clockrate = 800; //Somehow arbitrary, set me
    XML->sys.L2[0].device_type = 2;
  } else if (uncore->LLC) // LLC is L3
  {
    XML->sys.L3[0].L3_config[0] = uncore->LLC->sets * uncore->LLC->assoc * uncore->LLC->linesize;
    XML->sys.L3[0].L3_config[1] = uncore->LLC->linesize;
    XML->sys.L3[0].L3_config[2] = uncore->LLC->assoc;
    XML->sys.L3[0].L3_config[3] = 1;//uncore->LLC->banks;
    XML->sys.L3[0].L3_config[4] = 1;
    XML->sys.L3[0].L3_config[5] = uncore->LLC->latency;
    XML->sys.L3[0].L3_config[6] = uncore->LLC->bank_width;
    XML->sys.L3[0].L3_config[7] = (uncore->LLC->write_policy == WRITE_THROUGH) ? 0 : 1;

    XML->sys.L3[0].ports[0] = 1;
    XML->sys.L3[0].ports[1] = 0;
    XML->sys.L3[0].ports[2] = 0;

    XML->sys.L3[0].buffer_sizes[0] = 1;
    XML->sys.L3[0].buffer_sizes[1] = 2;
    XML->sys.L3[0].buffer_sizes[2] = 2;
    XML->sys.L3[0].buffer_sizes[3] = 2;

    XML->sys.L3[0].clockrate = 800; //Somehow arbitrary, set me
    XML->sys.L3[0].device_type = 0;
  }

  XML->sys.mc.number_mcs = 0;
  XML->sys.flashc.number_mcs = 0;
  XML->sys.niu.number_units = 0;
  XML->sys.pcie.number_units = 0;

  for (int i=0; i<num_threads; i++)
    cores[i]->power->translate_params(&XML->sys.core[i], &XML->sys.L2[i]);

  mcpat_initialize(XML, &cerr, 5);

  cores_rtp = (double*)calloc(num_threads, sizeof(*cores_rtp));
  if (cores_rtp == NULL)
    fatal("couldn't allocate memory");

  core_knobs_t* knobs = cores[0]->knobs;

  if (knobs->power.rtp_interval > 0)
  {
    assert(knobs->power.rtp_filename);
    rtp_file = fopen(knobs->power.rtp_filename, "w");
    if (rtp_file == NULL)
      fatal("couldn't open rtp power file: %s", knobs->power.rtp_filename);
  }
}

void deinit_power(void)
{
  free(cores_rtp);
  if (rtp_file)
    fclose(rtp_file);
  mcpat_finalize();
}

void translate_uncore_stats(struct stat_sdb_t* sdb, root_system* stats)
{
  struct stat_stat_t* curr_stat = NULL;

  if (uncore->LLC)
  {
    if (!private_l2)
    {
      curr_stat = stat_find_stat(sdb, "LLC.load_lookups");
      stats->L2[0].read_accesses = curr_stat->variant.for_sqword.end_val;
      curr_stat = stat_find_stat(sdb, "LLC.load_misses");
      stats->L2[0].read_misses = curr_stat->variant.for_sqword.end_val;
      curr_stat = stat_find_stat(sdb, "LLC.store_lookups");
      stats->L2[0].write_accesses = curr_stat->variant.for_sqword.end_val;
      curr_stat = stat_find_stat(sdb, "LLC.store_misses");
      stats->L2[0].write_misses = curr_stat->variant.for_sqword.end_val;
    }
    else {
      curr_stat = stat_find_stat(sdb, "LLC.load_lookups");
      stats->L3[0].read_accesses = curr_stat->variant.for_sqword.end_val;
      curr_stat = stat_find_stat(sdb, "LLC.load_misses");
      stats->L3[0].read_misses = curr_stat->variant.for_sqword.end_val;
      curr_stat = stat_find_stat(sdb, "LLC.store_lookups");
      stats->L3[0].write_accesses = curr_stat->variant.for_sqword.end_val;
      curr_stat = stat_find_stat(sdb, "LLC.store_misses");
      stats->L3[0].write_misses = curr_stat->variant.for_sqword.end_val;
    }
  }

  curr_stat = stat_find_stat(sdb, "sim_cycle");
  stats->total_cycles = curr_stat->variant.for_sqword.end_val;
}

void compute_power(struct stat_sdb_t* sdb, bool print_power)
{
  /* Get necessary simualtor stats */
  translate_uncore_stats(sdb, &XML->sys);
  for(int i=0; i<num_threads; i++)
    cores[i]->power->translate_stats(sdb, &XML->sys.core[i], &XML->sys.L2[i]);

  /* Invoke mcpat */
  mcpat_compute_energy(print_power, cores_rtp, &uncore_rtp);

  /* Print power trace */
  if (rtp_file)
  {
    for(int i=0; i<num_threads; i++)
        fprintf(rtp_file, "%.4f ", cores_rtp[i]);
    fprintf(rtp_file, "%.4f\n", uncore_rtp);
  }
}

void core_power_t::translate_params(system_core *core_params, system_L2 *L2_params)
{
  (void) L2_params;
  struct core_knobs_t *knobs = core->knobs;

  core_params->clock_rate = (int)uncore->cpu_speed;
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
    // Hardcode banks to 1, McPAT adds big overhead for multibanked caches
    core_params->dcache.dcache_config[3] = 1;
    core_params->dcache.dcache_config[4] = 1;
    core_params->dcache.dcache_config[5] = core->memory.DL1->latency;
    core_params->dcache.dcache_config[6] = core->memory.DL1->bank_width;
    core_params->dcache.dcache_config[7] = (core->memory.DL1->write_policy == WRITE_THROUGH) ? 0 : 1;


    core_params->dcache.buffer_sizes[0] = 4;//core->memory.DL1->MSHR_size;
    core_params->dcache.buffer_sizes[1] = 4;//core->memory.DL1->fill_num[0]; //XXX
    core_params->dcache.buffer_sizes[2] = 4;//core->memory.DL1->PFF_size;
    core_params->dcache.buffer_sizes[3] = 4;//core->memory.DL1->WBB_size;
  }

  if (core->memory.IL1)
  {
    core_params->icache.icache_config[0] = core->memory.IL1->sets * core->memory.IL1->assoc * core->memory.IL1->linesize;
    core_params->icache.icache_config[1] = core->memory.IL1->linesize;
    core_params->icache.icache_config[2] = core->memory.IL1->assoc;
    // Hardcode banks to 1, McPAT adds big overhead for multibanked caches
    core_params->icache.icache_config[3] = 1;
    core_params->icache.icache_config[4] = 1;
    core_params->icache.icache_config[5] = core->memory.IL1->latency;
    core_params->icache.icache_config[6] = core->memory.IL1->bank_width;
    core_params->icache.icache_config[7] = 1;//(core->memory.IL1->write_policy == WRITE_THROUGH) ? 0 : 1;

    core_params->icache.buffer_sizes[0] = 2;//core->memory.IL1->MSHR_size;
    core_params->icache.buffer_sizes[1] = 2;//core->memory.IL1->fill_num[0]; //XXX
    core_params->icache.buffer_sizes[2] = 2;//core->memory.IL1->PFF_size;
    core_params->icache.buffer_sizes[3] = 2;//core->memory.IL1->WBB_size;
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

    class bpred_dir_t *pred = core->fetch->bpred->get_dir_bpred()[0];
    core_params->predictor.local_predictor_entries  = pred->get_local_size();
    core_params->predictor.local_predictor_size[0]  = pred->get_local_width(0);
    core_params->predictor.local_predictor_size[1]  = pred->get_local_width(1);
    core_params->predictor.global_predictor_entries  = pred->get_global_size();
    core_params->predictor.global_predictor_bits  = pred->get_global_width();
    core_params->predictor.chooser_predictor_entries  = pred->get_chooser_size();
    core_params->predictor.chooser_predictor_bits  = pred->get_chooser_width();
  }

  core_params->pipeline_duty_cycle = 1;

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

void core_power_t::translate_stats(struct stat_sdb_t* sdb, system_core *core_stats, system_L2 *L2_stats)
{
  struct stat_stat_t *stat;
  int coreID = core->id;

  (void) L2_stats;

  stat = stat_find_core_stat(sdb, coreID, "oracle_total_uops");
  core_stats->total_instructions = stat->variant.for_sqword.end_val;

  stat = stat_find_core_stat(sdb, coreID, "oracle_total_branches");
  core_stats->branch_instructions = stat->variant.for_sqword.end_val;
  stat = stat_find_core_stat(sdb, coreID, "num_jeclear");
  core_stats->branch_mispredictions = stat->variant.for_sqword.end_val;
  stat = stat_find_core_stat(sdb, coreID, "oracle_total_loads");
  core_stats->load_instructions = stat->variant.for_sqword.end_val;
  stat = stat_find_core_stat(sdb, coreID, "oracle_total_refs");
  core_stats->store_instructions = stat->variant.for_sqword.end_val - core_stats->load_instructions;
  stat = stat_find_core_stat(sdb, coreID, "oracle_num_uops");
  core_stats->committed_instructions = stat->variant.for_sqword.end_val;

  stat = stat_find_stat(sdb, "sim_cycle");
  core_stats->total_cycles = stat->variant.for_sqword.end_val;
  core_stats->idle_cycles = 0;
  core_stats->busy_cycles = core_stats->total_cycles - core_stats->idle_cycles;

  stat = stat_find_core_stat(sdb, coreID, "regfile_reads");
  core_stats->int_regfile_reads = stat->variant.for_sqword.end_val;
  stat = stat_find_core_stat(sdb, coreID, "fp_regfile_reads");
  core_stats->float_regfile_reads = stat->variant.for_sqword.end_val;
  stat = stat_find_core_stat(sdb, coreID, "regfile_writes");
  core_stats->int_regfile_writes = stat->variant.for_sqword.end_val;
  stat = stat_find_core_stat(sdb, coreID, "fp_regfile_writes");
  core_stats->float_regfile_writes = stat->variant.for_sqword.end_val;

  stat = stat_find_core_stat(sdb, coreID, "oracle_total_calls");
  core_stats->function_calls = stat->variant.for_sqword.end_val;

  stat = stat_find_core_stat(sdb, coreID, "int_FU_occupancy");
  core_stats->cdb_alu_accesses = stat->variant.for_sqword.end_val;
  stat = stat_find_core_stat(sdb, coreID, "fp_FU_occupancy");
  core_stats->cdb_fpu_accesses = stat->variant.for_sqword.end_val;
  stat = stat_find_core_stat(sdb, coreID, "mul_FU_occupancy");
  core_stats->cdb_mul_accesses = stat->variant.for_sqword.end_val;
  core_stats->ialu_accesses = core_stats->cdb_alu_accesses;
  core_stats->fpu_accesses = core_stats->cdb_fpu_accesses;
  core_stats->mul_accesses = core_stats->cdb_mul_accesses;

  if (core->memory.ITLB)
  {
    stat = stat_find_core_stat(sdb, coreID, "ITLB.lookups");
    core_stats->itlb.total_accesses = stat->variant.for_sqword.end_val;
    stat = stat_find_core_stat(sdb, coreID, "ITLB.misses");
    core_stats->itlb.total_misses = stat->variant.for_sqword.end_val;
  }

  if (core->memory.DTLB)
  {
    stat = stat_find_core_stat(sdb, coreID, "DTLB.lookups");
    core_stats->dtlb.total_accesses = stat->variant.for_sqword.end_val;
    stat = stat_find_core_stat(sdb, coreID, "DTLB.misses");
    core_stats->dtlb.total_misses = stat->variant.for_sqword.end_val;
  }
  
  if (core->memory.IL1)
  {
    stat = stat_find_core_stat(sdb, coreID, "IL1.lookups");
    core_stats->icache.read_accesses = stat->variant.for_sqword.end_val;
    stat = stat_find_core_stat(sdb, coreID, "IL1.misses");
    core_stats->icache.read_misses = stat->variant.for_sqword.end_val;
  }

  if (core->memory.DL1)
  {
    stat = stat_find_core_stat(sdb, coreID, "DL1.load_lookups");
    core_stats->dcache.read_accesses = stat->variant.for_sqword.end_val;
    stat = stat_find_core_stat(sdb, coreID, "DL1.load_misses");
    core_stats->dcache.read_misses = stat->variant.for_sqword.end_val;
    stat = stat_find_core_stat(sdb, coreID, "DL1.store_lookups");
    core_stats->dcache.write_accesses = stat->variant.for_sqword.end_val;
    stat = stat_find_core_stat(sdb, coreID, "DL1.store_misses");
    core_stats->dcache.write_misses = stat->variant.for_sqword.end_val;
  }

  if (core->fetch->bpred->get_dir_btb())
  {
    stat = stat_find_core_stat(sdb, coreID, "BTB.lookups");
    core_stats->BTB.read_accesses = stat->variant.for_sqword.end_val;
    stat = stat_find_core_stat(sdb, coreID, "BTB.updates");
    core_stats->BTB.write_accesses = stat->variant.for_sqword.end_val;
    stat = stat_find_core_stat(sdb, coreID, "BTB.spec_updates");
    core_stats->BTB.write_accesses += stat->variant.for_sqword.end_val;
  }
}

/* load in all definitions */
#include "ZCORE-power.list"

/* default constructor */
core_power_t::core_power_t(struct core_t * _core):
  rt_power(0.0),
  core(_core)
{
}

/* default destructor */
core_power_t::~core_power_t(void)
{
}

class core_power_t * power_create(const char * power_opt_string, struct core_t * core)
{
#define ZESTO_PARSE_ARGS
#include "ZCORE-power.list"

  fatal("unknown power model type \"%s\"", power_opt_string);
#undef ZESTO_PARSE_ARGS
}
