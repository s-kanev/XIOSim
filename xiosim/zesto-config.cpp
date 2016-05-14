/* Read Zesto configuration file and store settings in the knobs structure.
 * See the header for any relevant documentation.
 *
 * Author: Sam Xi
 */

#include <cmath>
#include <iostream>
#include <string>
#include <sstream>

#include "confuse.h"

#include "core_const.h"
#include "misc.h"
#include "knobs.h"
#include "zesto-config.h"

using namespace xiosim;

static cfg_t* all_opts;

// Declaration of all configuration parameters.
extern cfg_opt_t system_cfg[];
extern cfg_opt_t iprefetch_cfg[];
extern cfg_opt_t itlb_cfg[];
extern cfg_opt_t icache_cfg[];
extern cfg_opt_t dprefetch_cfg[];
extern cfg_opt_t dtlb_cfg[];
extern cfg_opt_t d2tlb_cfg[];
extern cfg_opt_t dcache_cfg[];
extern cfg_opt_t l2prefetch_cfg[];
extern cfg_opt_t l2cache_cfg[];
extern cfg_opt_t llcprefetch_cfg[];
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
extern cfg_opt_t profiling_cfg[];
extern cfg_opt_t ignore_cfg[];
extern cfg_opt_t uncore_cfg[];
extern cfg_opt_t top_level_cfg[];

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
static void store_int_list(cfg_t* cfg, const char* attr_name, int* target, int* num_entries,
                           int max_entries) {
    *num_entries = fmin(max_entries, cfg_size(cfg, attr_name));
    for (int i = 0; i < *num_entries; i++) {
        target[i] = cfg_getnint(cfg, attr_name, i);
    }
}

/* Same as above, but supporting unlimited variable-length lists. */
static std::vector<int> store_int_list(cfg_t* cfg, const char* attr_name) {
    int num_entries = cfg_size(cfg, attr_name);
    std::vector<int> res;
    for (int i = 0; i < num_entries; i++) {
        res.push_back(cfg_getnint(cfg, attr_name, i));
    }
    return res;
}

/* Stores a string list config value into a preallocated array.
 *
 * Params:
 * @cfg: cfg_t object holding the config value.
 * @attr_name: Name of the configuration parameter.
 * @target: Preallocated string array of size @max_entries.
 * @num_entries: Pointer to an int where the number of values read will be
 * stored.
 * @max_entries: Maximum number of entries to store.
 */
static void store_str_list(cfg_t* cfg,
                           const char* attr_name,
                           const char* target[],
                           int* num_entries,
                           int max_entries) {
    *num_entries = fmin(max_entries, cfg_size(cfg, attr_name));
    for (int i = 0; i < *num_entries; i++) {
        target[i] = cfg_getnstr(cfg, attr_name, i);
    }
}

/* Same as above, but supporting unlimited variable-length lists. */
static std::vector<std::string> store_str_list(cfg_t* cfg, const char* attr_name) {
    int num_entries = cfg_size(cfg, attr_name);
    std::vector<std::string> res;
    for (int i = 0; i < num_entries; i++) {
        res.push_back(cfg_getnstr(cfg, attr_name, i));
    }
    return res;
}

/* Execution units consist of an execution latency, issue rate, and a set of
 * port bindings. This function parses the list of port bindings in the
 * configuration and copies the values into the appropriate knob, as well as
 * setting the other two parameters.
 *
 * Params:
 * @exec_cfg: Execution stage config.
 * @exeu_name: Execution unit name.
 * @fu_type: Functional unit type, enumerated in fu_class.
 * @knobs: Configuration knobs.
 */
static void store_execution_unit_options(cfg_t* exec_opt,
                                         const char* exeu_name,
                                         fu_class fu_type,
                                         core_knobs_t* knobs) {
    cfg_t* exeu_opt = cfg_gettsec(exec_opt, "exeu", exeu_name);
    // If this execution unit has not been declared, it doesn't exist in the
    // target system, so we should not set any defaults.
    if (!exeu_opt)
        return;
    knobs->exec.port_binding[fu_type].ports = store_int_list(exeu_opt, "port_binding");
    knobs->exec.port_binding[fu_type].num_FUs = knobs->exec.port_binding[fu_type].ports.size();
    if (knobs->exec.port_binding[fu_type].ports.size() == 0)
        fatal("no port bindings for %s", exeu_name);
    for (int port : knobs->exec.port_binding[fu_type].ports) {
        if (port < 0 || port >= knobs->exec.num_exec_ports)
            fatal("port binding for %s is negative or exceeds the execution width (should be > 0 "
                  "and < %d)",
                  exeu_name,
                  knobs->exec.num_exec_ports);
    }
    knobs->exec.latency[fu_type] = cfg_getint(exeu_opt, "latency");
    knobs->exec.issue_rate[fu_type] = cfg_getint(exeu_opt, "rate");
}

static void store_prefetcher_options(cfg_t* pf_opt, prefetcher_knobs_t* pf_knobs) {
    store_str_list(pf_opt, "config", pf_knobs->pf_opt_str, &pf_knobs->num_pf, MAX_PREFETCHERS);
    pf_knobs->pff_size = cfg_getint(pf_opt, "fifosize");
    pf_knobs->pf_thresh = cfg_getint(pf_opt, "threshold");
    pf_knobs->pf_max = cfg_getint(pf_opt, "max_outstanding_requests");
    pf_knobs->pf_buffer_size = cfg_getint(pf_opt, "buffer");
    pf_knobs->pf_filter_size = cfg_getint(pf_opt, "filter");
    pf_knobs->pf_filter_reset = cfg_getint(pf_opt, "filter_reset");
    pf_knobs->pf_on_miss = cfg_getbool(pf_opt, "on_miss_only");
    pf_knobs->low_watermark = cfg_getfloat(pf_opt, "watermark_min");
    pf_knobs->high_watermark = cfg_getfloat(pf_opt, "watermark_max");
    pf_knobs->watermark_interval = cfg_getint(pf_opt, "watermark_sampling_interval");
}

static void store_fetch_options(cfg_t* fetch_opt, core_knobs_t* knobs) {
    cfg_t* icache_opt = cfg_getsec(fetch_opt, "icache_cfg");
    cfg_t* icache_prefetch_opt = cfg_getsec(icache_opt, "iprefetch_cfg");
    cfg_t* itlb_opt = cfg_getsec(icache_opt, "itlb_cfg");
    cfg_t* branch_pred_opt = cfg_getsec(fetch_opt, "branch_pred_cfg");
    cfg_t* byte_queue_opt = cfg_getsec(fetch_opt, "byte_queue_cfg");
    cfg_t* predecode_opt = cfg_getsec(fetch_opt, "predecode_cfg");

    knobs->fetch.IQ_size = cfg_getint(fetch_opt, "instruction_queue_size");
    knobs->fetch.byteQ_size = cfg_getint(byte_queue_opt, "size");
    knobs->fetch.byteQ_linesize = cfg_getint(byte_queue_opt, "line_size");
    knobs->fetch.depth = cfg_getint(predecode_opt, "depth");
    knobs->fetch.width = cfg_getint(predecode_opt, "width");
    store_str_list(branch_pred_opt, "type", knobs->fetch.bpred_opt_str,
                   &knobs->fetch.num_bpred_components, MAX_HYBRID_BPRED);
    knobs->fetch.fusion_opt_str = cfg_getstr(branch_pred_opt, "fusion");
    knobs->fetch.dirjmpbtb_opt_str = cfg_getstr(branch_pred_opt, "btb");
    knobs->fetch.indirjmpbtb_opt_str = cfg_getstr(branch_pred_opt, "ibtb");
    knobs->fetch.ras_opt_str = cfg_getstr(branch_pred_opt, "ras");
    knobs->fetch.jeclear_delay = cfg_getint(branch_pred_opt, "jump_exec_delay");
    knobs->memory.IL1_opt_str = cfg_getstr(icache_opt, "config");
    knobs->memory.IL1_controller_opt_str = cfg_getstr(icache_opt, "coherency_controller");
    knobs->memory.IL1_magic_hit_rate = cfg_getfloat(icache_opt, "magic_hit_rate");
    knobs->memory.IL1_sample_misses = cfg_getbool(icache_opt, "sample_misses");
    store_prefetcher_options(icache_prefetch_opt, &knobs->memory.IL1_pf);

    knobs->memory.ITLB_opt_str = cfg_getstr(itlb_opt, "config");
    knobs->memory.ITLB_controller_opt_str = cfg_getstr(itlb_opt, "coherency_controller");
}

static void store_decode_options(cfg_t* decode_opt, core_knobs_t* knobs) {
    cfg_t* uop_fusion_opt = cfg_getsec(decode_opt, "uop_fusion_cfg");

    knobs->decode.depth = cfg_getint(decode_opt, "depth");
    knobs->decode.width = cfg_getint(decode_opt, "width");
    knobs->decode.target_stage = cfg_getint(decode_opt, "branch_agen_stage");
    knobs->decode.branch_decode_limit = cfg_getint(decode_opt, "branch_decode_limit");
    knobs->decode.max_uops = store_int_list(decode_opt, "decoder_max_uops");
    if (knobs->decode.max_uops.size() && knobs->decode.max_uops.size() != (size_t)knobs->decode.width)
        fatal("number of decoder_max_uops must be 0 or equal to decode pipeline width");
    if (knobs->decode.max_uops.size() == 0)
        knobs->decode.max_uops = std::vector<int>(knobs->decode.width, 0);
    knobs->decode.MS_latency = cfg_getint(decode_opt, "ucode_sequencer_latency");
    knobs->decode.uopQ_size = cfg_getint(decode_opt, "uop_queue_size");
    knobs->decode.fusion_mode.LOAD_OP = cfg_getbool(uop_fusion_opt, "load_comp_op");
    knobs->decode.fusion_mode.FP_LOAD_OP = cfg_getbool(uop_fusion_opt, "fpload_comp_op");
    knobs->decode.fusion_mode.STA_STD = cfg_getbool(uop_fusion_opt, "sta_std");
    knobs->decode.fusion_mode.LOAD_OP_ST = cfg_getbool(uop_fusion_opt, "load_op_store");
}

static void store_alloc_options(cfg_t* alloc_opt, core_knobs_t* knobs) {
    knobs->alloc.depth = cfg_getint(alloc_opt, "depth");
    knobs->alloc.width = cfg_getint(alloc_opt, "width");
    knobs->alloc.drain_flush = cfg_getbool(alloc_opt, "use_drain_flush");
}

static void store_exec_stage_options(cfg_t* exec_opt, core_knobs_t* knobs) {
    cfg_t* dcache_opt = cfg_getsec(exec_opt, "dcache_cfg");
    cfg_t* l2_opt = cfg_getsec(exec_opt, "l2cache_cfg");
    cfg_t* dtlb_opt = cfg_getsec(dcache_opt, "dtlb_cfg");
    cfg_t* d2tlb_opt = cfg_getsec(dcache_opt, "d2tlb_cfg");
    cfg_t* dpf_opt = cfg_getsec(dcache_opt, "dprefetch_cfg");
    cfg_t* l2pf_opt = cfg_getsec(l2_opt, "l2prefetch_cfg");
    cfg_t* repeater_opt = cfg_getsec(exec_opt, "repeater_cfg");

    knobs->exec.RS_size = cfg_getint(exec_opt, "rs_size");
    knobs->exec.LDQ_size = cfg_getint(exec_opt, "loadq_size");
    knobs->exec.STQ_size = cfg_getint(exec_opt, "storeq_size");
    knobs->exec.num_exec_ports = cfg_getint(exec_opt, "width");
    knobs->exec.payload_depth = cfg_getint(exec_opt, "payload_depth");
    knobs->exec.tornado_breaker = cfg_getbool(exec_opt, "enable_tornado_breaker");
    knobs->exec.throttle_partial = cfg_getbool(exec_opt, "enable_partial_throttle");
    knobs->exec.fp_penalty = cfg_getint(exec_opt, "fp_forward_penalty");
    knobs->exec.memdep_opt_str = cfg_getstr(exec_opt, "mem_dep_pred_config");

    knobs->memory.DL1_opt_str = cfg_getstr(dcache_opt, "config");
    knobs->memory.DL1_MSHR_cmd = cfg_getstr(dcache_opt, "mshr_cmd");
    knobs->memory.DL1_controller_opt_str = cfg_getstr(dcache_opt, "coherency_controller");
    knobs->memory.DL1_magic_hit_rate = cfg_getfloat(dcache_opt, "magic_hit_rate");
    knobs->memory.DL1_sample_misses = cfg_getbool(dcache_opt, "sample_misses");
    knobs->memory.DL2_opt_str = cfg_getstr(l2_opt, "config");
    knobs->memory.DL2_MSHR_cmd = cfg_getstr(l2_opt, "mshr_cmd");
    knobs->memory.DL2_controller_opt_str = cfg_getstr(l2_opt, "coherency_controller");
    knobs->memory.DL2_magic_hit_rate = cfg_getfloat(l2_opt, "magic_hit_rate");
    knobs->memory.DL2_sample_misses = cfg_getbool(l2_opt, "sample_misses");

    store_prefetcher_options(dpf_opt, &knobs->memory.DL1_pf);
    store_prefetcher_options(l2pf_opt, &knobs->memory.DL2_pf);

    knobs->memory.DTLB_opt_str = cfg_getstr(dtlb_opt, "config");
    knobs->memory.DTLB_controller_opt_str = cfg_getstr(dtlb_opt, "coherency_controller");
    knobs->memory.DTLB2_opt_str = cfg_getstr(d2tlb_opt, "config");
    knobs->memory.DTLB2_controller_opt_str = cfg_getstr(d2tlb_opt, "coherency_controller");

    knobs->exec.repeater_opt_str = cfg_getstr(repeater_opt, "config");
    knobs->memory.DL1_rep_req = cfg_getbool(repeater_opt, "request_dl1");

    store_execution_unit_options(exec_opt, "int_alu", FU_IEU, knobs);
    store_execution_unit_options(exec_opt, "jump", FU_JEU, knobs);
    store_execution_unit_options(exec_opt, "int_mul", FU_IMUL, knobs);
    store_execution_unit_options(exec_opt, "int_div", FU_IDIV, knobs);
    store_execution_unit_options(exec_opt, "shift", FU_SHIFT, knobs);
    store_execution_unit_options(exec_opt, "fp_alu", FU_FADD, knobs);
    store_execution_unit_options(exec_opt, "fp_mul", FU_FMUL, knobs);
    store_execution_unit_options(exec_opt, "fp_div", FU_FDIV, knobs);
    store_execution_unit_options(exec_opt, "fp_cplx", FU_FCPLX, knobs);
    store_execution_unit_options(exec_opt, "ld", FU_LD, knobs);
    store_execution_unit_options(exec_opt, "st_agen", FU_STA, knobs);
    store_execution_unit_options(exec_opt, "st_data", FU_STD, knobs);
    store_execution_unit_options(exec_opt, "lea", FU_AGEN, knobs);
    store_execution_unit_options(exec_opt, "magic", FU_MAGIC, knobs);
}

static void store_commit_options(cfg_t* commit_opt, core_knobs_t* knobs) {
    knobs->commit.ROB_size = cfg_getint(commit_opt, "rob_size");
    knobs->commit.width = cfg_getint(commit_opt, "commit_width");
    knobs->commit.branch_limit = cfg_getint(commit_opt, "commit_branches");
    knobs->commit.pre_commit_depth = cfg_getint(commit_opt, "precommit_depth");
}

static void store_core_options(cfg_t* core_opt, core_knobs_t* knobs) {
    cfg_t* fetch_opt = cfg_getsec(core_opt, "fetch_cfg");
    cfg_t* decode_opt = cfg_getsec(core_opt, "decode_cfg");
    cfg_t* alloc_opt = cfg_getsec(core_opt, "alloc_cfg");
    cfg_t* exec_opt = cfg_getsec(core_opt, "exec_cfg");
    cfg_t* commit_opt = cfg_getsec(core_opt, "commit_cfg");
    knobs->model = cfg_getstr(core_opt, "pipeline_model");
    knobs->default_cpu_speed = cfg_getfloat(core_opt, "core_clock");
    store_fetch_options(fetch_opt, knobs);
    store_decode_options(decode_opt, knobs);
    store_alloc_options(alloc_opt, knobs);
    store_exec_stage_options(exec_opt, knobs);
    store_commit_options(commit_opt, knobs);
}

static void store_uncore_options(cfg_t* uncore_opt, uncore_knobs_t* knobs) {
    cfg_t* llccache_opt = cfg_getsec(uncore_opt, "llccache_cfg");
    cfg_t* llcprefetch_opt = cfg_getsec(llccache_opt, "llcprefetch_cfg");
    cfg_t* fsb_opt = cfg_getsec(uncore_opt, "fsb_cfg");
    cfg_t* dram_opt = cfg_getsec(uncore_opt, "dram_cfg");

    knobs->LLC_opt_str = cfg_getstr(llccache_opt, "config");
    knobs->LLC_MSHR_cmd = cfg_getstr(llccache_opt, "mshr_cmd");
    knobs->LLC_speed = cfg_getfloat(llccache_opt, "clock");
    knobs->LLC_controller_str = cfg_getstr(llccache_opt, "coherency_controller");
    knobs->LLC_magic_hit_rate = cfg_getfloat(llccache_opt, "magic_hit_rate");
    knobs->LLC_sample_misses = cfg_getbool(llccache_opt, "sample_misses");

    store_prefetcher_options(llcprefetch_opt, &knobs->LLC_pf);

    knobs->fsb_width = cfg_getint(fsb_opt, "width");
    knobs->fsb_DDR = cfg_getbool(fsb_opt, "ddr");
    knobs->fsb_speed = cfg_getfloat(fsb_opt, "clock");
    knobs->fsb_magic = cfg_getbool(fsb_opt, "magic");

    knobs->MC_opt_string = cfg_getstr(dram_opt, "memory_controller_config");
    knobs->dram_opt_string = cfg_getstr(dram_opt, "dram_config");
}

static void store_system_options(cfg_t* system_opt, system_knobs_t* knobs) {
    knobs->assert_spin = cfg_getbool(system_opt, "assert_spin");
    knobs->rand_seed = cfg_getint(system_opt, "seed");
    knobs->num_cores = cfg_getint(system_opt, "num_cores");
    if ((knobs->num_cores < 1) || (knobs->num_cores > MAX_CORES))
        fatal("-cores must be between 1 and %d (inclusive)", MAX_CORES);
    knobs->heartbeat_frequency = cfg_getint(system_opt, "heartbeat_interval");
    knobs->ztrace_filename = cfg_getstr(system_opt, "ztrace_file_prefix");
    knobs->sim_simout = cfg_getstr(system_opt, "output_redir");

    knobs->power.compute = cfg_getbool(system_opt, "simulate_power");
    knobs->power.rtp_interval = cfg_getint(system_opt, "power_rtp_interval");
    knobs->power.rtp_filename = cfg_getstr(system_opt, "power_rtp_file");

    knobs->cache_miss_sample_parameter = cfg_getint(system_opt, "cache_miss_sample_parameter");

    cfg_t* profiling_opt = cfg_getsec(system_opt, "profiling_cfg");
    knobs->profiling_file_prefix = cfg_getstr(profiling_opt, "file_prefix");
    knobs->profiling_start = store_str_list(profiling_opt, "start");
    knobs->profiling_stop = store_str_list(profiling_opt, "stop");
    if ((knobs->profiling_start.size() != knobs->profiling_stop.size()) &&
        knobs->profiling_stop.size() > 0)
        fatal("profiling.start and profiling.stop must be equal size, or profiling.stop zero");

    cfg_t* ignore_opt = cfg_getsec(system_opt, "ignore_cfg");
    knobs->ignored_funcs = store_str_list(ignore_opt, "funcs");
    knobs->ignored_pcs = store_str_list(ignore_opt, "pcs");

    cfg_t* dvfs_opt = cfg_getsec(system_opt, "dvfs_cfg");
    knobs->dvfs_opt_str = cfg_getstr(dvfs_opt, "config");
    knobs->dvfs_interval = cfg_getint(dvfs_opt, "interval");

    cfg_t* scheduler_opt = cfg_getsec(system_opt, "scheduler_cfg");
    knobs->scheduler_tick = cfg_getint(scheduler_opt, "scheduler_tick");
    knobs->allocator = cfg_getstr(scheduler_opt, "allocator");
    knobs->allocator_opt_target = cfg_getstr(scheduler_opt, "allocator_opt_target");
    knobs->speedup_model = cfg_getstr(scheduler_opt, "speedup_model");
}

void read_config_file(std::string cfg_file, core_knobs_t* core_knobs, uncore_knobs_t* uncore_knobs,
                      system_knobs_t* system_knobs) {
    all_opts = cfg_init(top_level_cfg, CFGF_NOCASE);
    int ret = cfg_parse(all_opts, cfg_file.c_str());
    if (ret == CFG_FILE_ERROR) {
        fatal("Failed to open configuration file %s", cfg_file.c_str());
    } else if (ret == CFG_PARSE_ERROR) {
        fatal("Config file parsing error.");
    }

    cfg_t* system_opt = cfg_getsec(all_opts, "system_cfg");
    cfg_t* core_opt = cfg_getsec(all_opts, "core_cfg");
    cfg_t* uncore_opt = cfg_getsec(all_opts, "uncore_cfg");

    store_system_options(system_opt, system_knobs);
    store_core_options(core_opt, core_knobs);
    store_uncore_options(uncore_opt, uncore_knobs);
}

void print_config(FILE* fd) { cfg_print(all_opts, fd); }

void free_config() { cfg_free(all_opts); }
