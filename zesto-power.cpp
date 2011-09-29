#include "thread.h"

#include "stats.h"
#include "zesto-power.h"
#include "zesto-core.h"
#include "zesto-fetch.h"
#include "zesto-bpred.h"
#include "zesto-uncore.h"

#include "XML_Parse.h"
#include "mcpat.h"

class ParseXML *XML = NULL; //Interface to McPAT

extern int num_threads;
extern struct stat_sdb_t *sim_sdb;
double *cores_rtp = NULL;
double uncore_rtp;

void init_power(void)
{
  XML = new ParseXML();
  XML->initialize();

  XML->sys.number_of_cores = num_threads;
  XML->sys.number_of_L1Directories = 0;
  XML->sys.number_of_L2Directories = 0;
  XML->sys.number_of_L2s = 1;
  XML->sys.Private_L2 = false;
  XML->sys.number_of_L3s = 0;
  XML->sys.number_of_NoCs = 0;
  XML->sys.number_of_dir_levels = 0;
  XML->sys.homogeneous_cores = 0;
  XML->sys.homogeneous_L1Directories = 0;
  XML->sys.homogeneous_L2Directories = 0;
  XML->sys.core_tech_node = 45;
  XML->sys.target_core_clockrate = uncore->cpu_speed;
  XML->sys.temperature = 380; // K
  XML->sys.device_type = 2;//0;//1; // Low leakage
  XML->sys.number_cache_levels = 1;
  XML->sys.machine_bits = 64;
  XML->sys.virtual_address_width = 64;
  XML->sys.physical_address_width = 52;
  XML->sys.virtual_memory_page_size = 4096;

  if (uncore->LLC)
  {
    XML->sys.L2[0].L2_config[0] = uncore->LLC->sets * uncore->LLC->assoc * uncore->LLC->linesize;
    fprintf(stderr, "LLC: %f ", XML->sys.L2[0].L2_config[0]);
    XML->sys.L2[0].L2_config[1] = uncore->LLC->linesize;
    fprintf(stderr, "%f ", XML->sys.L2[0].L2_config[1]);
    XML->sys.L2[0].L2_config[2] = uncore->LLC->assoc;
    fprintf(stderr, "%f ", XML->sys.L2[0].L2_config[2]);
    XML->sys.L2[0].L2_config[3] = uncore->LLC->banks;
    fprintf(stderr, "%f ", XML->sys.L2[0].L2_config[3]);
    XML->sys.L2[0].L2_config[4] = 1;//16;//8;//core->memory.IL1->; //XXX
    fprintf(stderr, "%f ", XML->sys.L2[0].L2_config[4]);
    XML->sys.L2[0].L2_config[5] = uncore->LLC->latency;
    fprintf(stderr, "%f ", XML->sys.L2[0].L2_config[5]);
    XML->sys.L2[0].L2_config[6] = uncore->LLC->bank_width;
    fprintf(stderr, "%f ", XML->sys.L2[0].L2_config[6]);
    XML->sys.L2[0].L2_config[7] = (uncore->LLC->write_policy == WRITE_THROUGH) ? 0 : 1;
    fprintf(stderr, "%f\n", XML->sys.L2[0].L2_config[7]);

    XML->sys.L2[0].ports[0] = 1;
    XML->sys.L2[0].ports[1] = 1;
    XML->sys.L2[0].ports[2] = 1;

    XML->sys.L2[0].clockrate = 400;
  }

  XML->sys.mc.number_mcs = 0;
  XML->sys.flashc.number_mcs = 0;
  XML->sys.niu.number_units = 0;
  XML->sys.pcie.number_units = 0;

  for (int i=0; i<num_threads; i++)
    cores[i]->power->translate_params(&XML->sys.core[i]);

  mcpat_initialize(XML, &cerr, 5);

  cores_rtp = (double*)calloc(num_threads, sizeof(*cores_rtp));
  if (cores_rtp == NULL)
    fatal("couldn't allocate memory");
}

void deinit_power(void)
{
  free(cores_rtp);
  mcpat_finalize();
}

void translate_uncore_stats(root_system* stats)
{
  struct stat_stat_t* curr_stat = NULL;

  if (uncore->LLC)
  {
    curr_stat = stat_find_stat(sim_sdb, "LLC.load_lookups");
    stats->L2[0].read_accesses = *curr_stat->variant.for_int.var;
    curr_stat = stat_find_stat(sim_sdb, "LLC.load_misses");
    stats->L2[0].read_misses = *curr_stat->variant.for_int.var;
    curr_stat = stat_find_stat(sim_sdb, "LLC.store_lookups");
    stats->L2[0].write_accesses = *curr_stat->variant.for_int.var;
    curr_stat = stat_find_stat(sim_sdb, "LLC.store_misses");
    stats->L2[0].write_misses = *curr_stat->variant.for_int.var;
  }

  curr_stat = stat_find_stat(sim_sdb, "sim_cycle");
  stats->total_cycles = *curr_stat->variant.for_int.var;
}

void compute_power(void)
{
  translate_uncore_stats(&XML->sys);

  for(int i=0; i<num_threads; i++)
    cores[i]->power->translate_stats(&XML->sys.core[i]);

//  mcpat_compute_energy(false, cores_rtp, &uncore_rtp);
  mcpat_compute_energy(true, cores_rtp, &uncore_rtp);
}

/* load in all definitions */
#include "ZCORE-power.list"

/* default constructor */
core_power_t::core_power_t(void):
  rt_power(0.0)
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
