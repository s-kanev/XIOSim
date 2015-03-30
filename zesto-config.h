/* Read Zesto configuration file and store settings in the knobs structure.
 *
 * Cache, TLB, and prefetcher configurations are identical from an attributes
 * perspective. However, they differ in their default values. Therefore, each
 * such structure has its own specification.
 *
 * Author: Sam Xi
 */

#ifndef __ZESTO_CONFIG_H__
#define __ZESTO_CONFIG_H__

#include "machine.h"  // Make sure this doesn't interfere with boost interprocess.
#include "confuse.h"
#include "zesto-structs.h"

// Global configuration variables. This might become unnecessary if I move all
// the functions into their original files.
extern int num_cores;
extern int heartbeat_frequency;
extern const char* ztrace_filename;
extern bool simulate_power;

extern int rand_seed;
extern bool help_me;
#ifdef DEBUG
extern bool debugging;
#endif
extern const char* sim_simout;
extern const char * LLC_opt_str;
extern const char * LLC_PF_opt_str[MAX_PREFETCHERS];
extern const char * LLC_MSHR_cmd;
extern const char * LLC_controller_str;
extern const char * MC_opt_string;
// Static
extern float LLC_magic_hit_rate;
extern int LLC_num_PF;
extern int LLC_PFFsize;
extern int LLC_PFthresh;
extern int LLC_PFmax;
extern int LLC_PF_buffer_size;
extern int LLC_PF_filter_size;
extern int LLC_PF_filter_reset;
extern int LLC_WMinterval;
extern bool LLC_PF_on_miss;
extern double LLC_low_watermark;
extern double LLC_high_watermark;
extern int fsb_width;
extern bool fsb_DDR;
extern double fsb_speed;

extern double LLC_speed;
extern bool fsb_magic;
extern const char* dram_opt_string;

// Declaration of all configuration parameters.
extern cfg_t *all_opts;
extern cfg_opt_t system_cfg[];
extern cfg_opt_t iprefetch_cfg[];
extern cfg_opt_t itlb_cfg[];
extern cfg_opt_t icache_cfg[];
extern cfg_opt_t dprefetch_cfg[];
extern cfg_opt_t dtlb_cfg[];
extern cfg_opt_t dcache_cfg[];
extern cfg_opt_t l2prefetch_cfg[];
extern cfg_opt_t l2tlb_cfg[];
extern cfg_opt_t l2cache_cfg[];
extern cfg_opt_t llcprefetch_cfg[];
extern cfg_opt_t llctlb_cfg[];
extern cfg_opt_t llccache_cfg[];
extern cfg_opt_t byte_queue_cfg[];
extern cfg_opt_t predecode_cfg[];
extern cfg_opt_t branch_pred_cfg[];
extern cfg_opt_t fetch_cfg[];
extern cfg_opt_t uop_fusion_cfg[];
extern cfg_opt_t decode_cfg[];
extern cfg_opt_t alloc_cfg[];
extern cfg_opt_t repeater_cfg[];
extern cfg_opt_t exeu_cfg[];
extern cfg_opt_t exec_cfg[];
extern cfg_opt_t commit_cfg[];
extern cfg_opt_t core_cfg[];
extern cfg_opt_t fsb_cfg[];
extern cfg_opt_t dram_cfg[];
extern cfg_opt_t dvfs_cfg[];
extern cfg_opt_t scheduler_cfg[];
extern cfg_opt_t uncore_cfg[];
extern cfg_opt_t top_level_cfg[];

/* Entry point for parsing Zesto configuration file. This function expects the
 * Zesto configuration file path to be in the flag "-config". Options are
 * stored in the knobs struct.
 */
int read_config_file(int argc, const char* argv[], core_knobs_t* knobs);

/* Primary pipeline configuration function declarations. */
void store_system_options(cfg_t *system_opt, core_knobs_t *knobs);
void store_core_options(cfg_t *core_opt, core_knobs_t *knobs);
void store_fetch_options(cfg_t *fetch_opt, core_knobs_t *knobs);
void store_decode_options(cfg_t *decode_opt, core_knobs_t *knobs);
void store_alloc_options(cfg_t *alloc_opt, core_knobs_t *knobs);
void store_exec_stage_options(cfg_t *exec_opt, core_knobs_t *knobs);
void store_commit_options(cfg_t *commit_opt, core_knobs_t* knobs);
void store_uncore_options(cfg_t *uncore_opt, core_knobs_t *knobs);

/* Execution units consist of an execution latency, issue rate, and a set of
 * port bindings. This function parses the list of port bindings in the
 * configuration and copies the values into the appropriate knob, as well as
 * setting the other two parameters.
 *
 * Params:
 * @exec_cfg: Execution stage config.
 * @exeu_name: Execution unit name.
 * @fu_type: Functional unit type, enumerated in md_fu_class.
 * @knobs: Configuration knobs.
 */
void store_execution_unit_options(cfg_t *exec_cfg,
                                  const char* exeu_name,
                                  md_fu_class fu_type,
                                  core_knobs_t *knobs);

/* Stores an int list config value into a preallocated array.
 *
 * Params:
 * @cfg: cfg_t object holding the config value.
 * @attr_name: Name of the configuration parameter.
 * @target: Preallocated int array of size @max_entries.
 * @num_entries: Pointer to an int where the number of values read will be
 * stored.
 * @max_entries: Maximum number of entries to store.
 */
void store_int_list(cfg_t* cfg,
                    const char* attr_name,
                    int* target,
                    int* num_entries,
                    int max_entries);

/* Stores an string list config value into a preallocated array.
 *
 * Params:
 * @cfg: cfg_t object holding the config value.
 * @attr_name: Name of the configuration parameter.
 * @target: Preallocated string array of size @max_entries.
 * @num_entries: Pointer to an int where the number of values read will be
 * stored.
 * @max_entries: Maximum number of entries to store.
 */
void store_str_list(cfg_t* cfg,
                    const char* attr_name,
                    const char* target[],
                    int* num_entries,
                    int max_entries);

#endif
