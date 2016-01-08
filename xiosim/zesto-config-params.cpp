/* Definitions of all configuration parameters.
 *
 * Author: Sam Xi
 */

#include "confuse.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"

// Don't forget to cfg_free() this when simulation is completed!
cfg_t *all_opts;

// Global settings about the system and the simulation.
cfg_opt_t system_cfg[] {
  CFG_BOOL("help", cfg_false, CFGF_NONE),
  CFG_BOOL("assert_spin", cfg_false, CFGF_NONE),
  CFG_INT("seed", 1, CFGF_NONE),
  CFG_INT("num_cores", 1, CFGF_NONE),
  CFG_INT("heartbeat_interval", 0, CFGF_NONE),
  CFG_STR("pipeline_model", "DPM", CFGF_NONE),  // Add callback
  CFG_STR("ztrace_file_prefix", "ztrace", CFGF_NONE),
  CFG_BOOL("simulate_power", cfg_false, CFGF_NONE),
  CFG_INT("power_rtp_interval", 0, CFGF_NONE),
  CFG_STR("power_rtp_file", "", CFGF_NONE),
  CFG_STR("output_redir", "sim.out", CFGF_NONE),
  CFG_INT("stopwatch_start_pc", 0, CFGF_NONE),
  CFG_INT("stopwatch_stop_pc", 0, CFGF_NONE),
  CFG_END()
};

/********************************************/
/********* Instruction cache ****************/
/********************************************/
cfg_opt_t iprefetch_cfg[] {
  CFG_STR_LIST("config", "none", CFGF_NONE),
  CFG_BOOL("on_miss_only", cfg_true, CFGF_NONE),
  CFG_INT("fifosize", 8, CFGF_NONE),
  CFG_INT("buffer", 0, CFGF_NONE),
  CFG_INT("filter", 0, CFGF_NONE),
  CFG_INT("filter_reset", 65536, CFGF_NONE),
  CFG_INT("threshold", 4, CFGF_NONE),
  CFG_INT("max_outstanding_requests", 2, CFGF_NONE),
  CFG_INT("watermark_sampling_interval", 100, CFGF_NONE),
  CFG_FLOAT("watermark_min", 0.1, CFGF_NONE),
  CFG_FLOAT("watermark_max", 0.3, CFGF_NONE),
  CFG_END()
};

cfg_opt_t itlb_cfg[] {
  CFG_STR("config", "ITLB:32:4:1:3:L:1", CFGF_NONE),
  CFG_STR("coherency_controller", "none", CFGF_NONE),
  CFG_END()
};

cfg_opt_t icache_cfg[] {
  CFG_STR("config", "IL1:64:8:64:4:16:3:L:8", CFGF_NONE),
  CFG_STR("coherency_controller", "none", CFGF_NONE),
  CFG_FLOAT("magic_hit_rate", -1.0, CFGF_NONE),
  CFG_STR("mshr_cmd", "none", CFGF_NONE),
  CFG_FLOAT("clock", 0, CFGF_NONE),
  CFG_SEC("iprefetch_cfg", iprefetch_cfg, CFGF_TITLE),
  CFG_SEC("itlb_cfg", itlb_cfg, CFGF_TITLE),
  CFG_END()
};

/********************************************/
/************* L1 data cache ****************/
/********************************************/
cfg_opt_t dprefetch_cfg[] {
  CFG_STR_LIST("config", "nextline", CFGF_NONE),
  CFG_BOOL("on_miss_only", cfg_false, CFGF_NONE),
  CFG_INT("fifosize", 8, CFGF_NONE),
  CFG_INT("buffer", 0, CFGF_NONE),
  CFG_INT("filter", 0, CFGF_NONE),
  CFG_INT("filter_reset", 65536, CFGF_NONE),
  CFG_INT("threshold", 4, CFGF_NONE),
  CFG_INT("max_outstanding_requests", 2, CFGF_NONE),
  CFG_INT("watermark_sampling_interval", 100, CFGF_NONE),
  CFG_FLOAT("watermark_min", 0.3, CFGF_NONE),
  CFG_FLOAT("watermark_max", 0.7, CFGF_NONE),
  CFG_END()
};

cfg_opt_t dtlb_cfg[] {
  CFG_STR("config", "DTLB:4:4:1:2:L:4", CFGF_NONE),
  CFG_STR("coherency_controller", "none", CFGF_NONE),
  CFG_END()
};

cfg_opt_t d2tlb_cfg[] {
  CFG_STR("config", "none", CFGF_NONE),
  CFG_STR("coherency_controller", "none", CFGF_NONE),
  CFG_END()
};

cfg_opt_t dcache_cfg[] {
  CFG_STR("config", "DL1:64:8:64:8:64:2:L:W:T:8:C", CFGF_NONE),
  CFG_STR("coherency_controller", "none", CFGF_NONE),  // CB
  CFG_FLOAT("magic_hit_rate", -1.0, CFGF_NONE),
  CFG_STR("mshr_cmd", "RWPB", CFGF_NONE),
  CFG_FLOAT("clock", 0, CFGF_NONE),
  CFG_SEC("dprefetch_cfg", dprefetch_cfg, CFGF_TITLE),
  CFG_SEC("dtlb_cfg", dtlb_cfg, CFGF_TITLE),
  CFG_SEC("d2tlb_cfg", d2tlb_cfg, CFGF_TITLE),
  CFG_END()
};

/********************************************/
/***************** L2 cache *****************/
/********************************************/
cfg_opt_t l2prefetch_cfg[] {
  CFG_STR_LIST("config", "nextline", CFGF_NONE),
  CFG_BOOL("on_miss_only", cfg_true, CFGF_NONE),
  CFG_INT("fifosize", 8, CFGF_NONE),
  CFG_INT("buffer", 0, CFGF_NONE),
  CFG_INT("filter", 0, CFGF_NONE),
  CFG_INT("filter_reset", 65536, CFGF_NONE),
  CFG_INT("threshold", 4, CFGF_NONE),
  CFG_INT("max_outstanding_requests", 2, CFGF_NONE),
  CFG_INT("watermark_sampling_interval", 100, CFGF_NONE),
  CFG_FLOAT("watermark_min", 0.3, CFGF_NONE),
  CFG_FLOAT("watermark_max", 0.7, CFGF_NONE),
  CFG_END()
};

cfg_opt_t l2cache_cfg[] {
  CFG_STR("config", "DL2:512:8:64:8:64:9:L:W:T:8:C", CFGF_NONE),
  CFG_STR("coherency_controller", "none", CFGF_NONE),  // CB
  CFG_FLOAT("magic_hit_rate", -1.0, CFGF_NONE),
  CFG_STR("mshr_cmd", "RPWB", CFGF_NONE),
  CFG_FLOAT("clock", 0, CFGF_NONE),
  CFG_SEC("l2prefetch_cfg", l2prefetch_cfg, CFGF_TITLE),
  CFG_END()
};

/********************************************/
/********** Last level cache ****************/
/********************************************/
cfg_opt_t llcprefetch_cfg[] {
  CFG_STR_LIST("config", "none", CFGF_NONE),
  CFG_BOOL("on_miss_only", cfg_false, CFGF_NONE),
  CFG_INT("fifosize", 16, CFGF_NONE),
  CFG_INT("buffer", 0, CFGF_NONE),
  CFG_INT("filter", 0, CFGF_NONE),
  CFG_INT("filter_reset", 65536, CFGF_NONE),
  CFG_INT("threshold", 4, CFGF_NONE),
  CFG_INT("max_outstanding_requests", 2, CFGF_NONE),
  CFG_INT("watermark_sampling_interval", 100, CFGF_NONE),
  CFG_FLOAT("watermark_min", 0.1, CFGF_NONE),
  CFG_FLOAT("watermark_max", 0.5, CFGF_NONE),
  CFG_END()
};

cfg_opt_t llccache_cfg[] {
  CFG_STR("config", "LLC:2048:16:64:16:64:12:L:W:B:8:1:8:C", CFGF_NONE),
  CFG_STR("coherency_controller", "none", CFGF_NONE),  // CB
  CFG_STR("mshr_cmd", "RPWB", CFGF_NODEFAULT),
  CFG_FLOAT("magic_hit_rate", -1.0, CFGF_NONE),
  CFG_FLOAT("clock", 800.0, CFGF_NONE),
  CFG_SEC("llcprefetch_cfg", llcprefetch_cfg, CFGF_TITLE),
  CFG_END()
};

/********************************************/
/*********** Pipeline stages ****************/
/********************************************/
cfg_opt_t byte_queue_cfg[] {
  CFG_INT("size", 4, CFGF_NONE),
  CFG_INT("line_size", 16, CFGF_NONE),
  CFG_END()
};

cfg_opt_t predecode_cfg[] {
  CFG_INT("depth", 2, CFGF_NONE),
  CFG_INT("width", 4, CFGF_NONE),
  CFG_END()
};

cfg_opt_t branch_pred_cfg[] {
  CFG_STR_LIST("type", "2lev:gshare:1:1024:6:1", CFGF_NONE),
  CFG_STR("fusion", "none", CFGF_NONE),
  CFG_STR("btb", "btac:BTB:1024:4:8:l", CFGF_NONE),
  CFG_STR("ibtb", "none", CFGF_NONE),
  CFG_STR("ras", "stack:RAS:16", CFGF_NONE),
  CFG_INT("jump_exec_delay", 1, CFGF_NONE),
  CFG_END()
};

cfg_opt_t fetch_cfg[] {
  CFG_INT("instruction_queue_size", 8, CFGF_NONE),
  CFG_SEC("icache_cfg", icache_cfg, CFGF_TITLE),
  CFG_SEC("branch_pred_cfg", branch_pred_cfg, CFGF_NONE),
  CFG_SEC("byte_queue_cfg", byte_queue_cfg, CFGF_NONE),
  CFG_SEC("predecode_cfg", predecode_cfg, CFGF_NONE),
  CFG_END()
};

cfg_opt_t uop_fusion_cfg[] {
  CFG_BOOL("load_comp_op", cfg_false, CFGF_NONE),
  CFG_BOOL("fpload_comp_op", cfg_false, CFGF_NONE),
  CFG_BOOL("sta_std", cfg_false, CFGF_NONE),
  CFG_BOOL("load_op_store", cfg_false, CFGF_NONE),
  CFG_END()
};

cfg_opt_t decode_cfg[] {
  CFG_INT("depth", 3, CFGF_NONE),
  CFG_INT("width", 4, CFGF_NONE),
  CFG_INT("branch_agen_stage", 1, CFGF_NONE),
  CFG_INT("branch_decode_limit", 1, CFGF_NONE),
  CFG_INT_LIST("decoder_max_uops", "{4, 1, 1, 1}", CFGF_NONE),
  CFG_INT("ucode_sequencer_latency", 0, CFGF_NONE),
  CFG_INT("uop_queue_size", 8, CFGF_NONE),
  CFG_SEC("uop_fusion_cfg", uop_fusion_cfg, CFGF_NONE),
  CFG_END()
};

cfg_opt_t alloc_cfg[] {
  CFG_INT("depth", 2, CFGF_NONE),
  CFG_INT("width", 4, CFGF_NONE),
  CFG_BOOL("use_drain_flush", cfg_false, CFGF_NONE),
  CFG_END()
};

cfg_opt_t repeater_cfg[] {
  CFG_STR("config", "none", CFGF_NONE),
  CFG_BOOL("request_dl1", cfg_false, CFGF_NONE),
  CFG_END()
};

// Execution unit configuration.
// Different execution units have different defaults but most of them have the
// configurations below.
cfg_opt_t exeu_cfg[] {
  CFG_INT("latency", 1, CFGF_NONE),
  CFG_INT("rate", 1, CFGF_NONE),
  CFG_INT_LIST("port_binding", "{0}", CFGF_NONE),
  CFG_END()
};

// Execution pipeline stage configuration.
cfg_opt_t exec_cfg[] {
  CFG_INT("width", 4, CFGF_NONE),
  CFG_INT("payload_depth", 1, CFGF_NONE),
  CFG_BOOL("enable_tornado_breaker", cfg_false, CFGF_NONE),
  CFG_BOOL("enable_partial_throttle", cfg_true, CFGF_NONE),
  CFG_INT("fp_forward_penalty", 0, CFGF_NONE),
  CFG_STR("mem_dep_pred_config", "lwt:LWT:4096:13107", CFGF_NONE),
  CFG_INT("rs_size", 20, CFGF_NONE),
  CFG_INT("loadq_size", 20, CFGF_NONE),
  CFG_INT("storeq_size", 16, CFGF_NONE),
  CFG_SEC("dcache_cfg", dcache_cfg, CFGF_TITLE),  // L1 and L2 dcaches.
  CFG_SEC("l2cache_cfg", l2cache_cfg, CFGF_TITLE),  // L1 and L2 dcaches.
  CFG_SEC("repeater_cfg", repeater_cfg, CFGF_NONE),
  CFG_SEC("exeu", exeu_cfg, CFGF_TITLE | CFGF_MULTI),
  CFG_END()
};

cfg_opt_t commit_cfg[] {
  CFG_INT("rob_size", 64, CFGF_NONE),
  CFG_INT("commit_width", 4, CFGF_NONE),
  CFG_INT("commit_branches", 2, CFGF_NONE),
  CFG_INT("precommit_depth", 2, CFGF_NONE),
  CFG_END()
};

cfg_opt_t core_cfg[] {
  CFG_FLOAT("core_clock", 4000.0, CFGF_NONE),
  CFG_SEC("fetch_cfg", fetch_cfg, CFGF_NONE),
  CFG_SEC("decode_cfg", decode_cfg, CFGF_NONE),
  CFG_SEC("alloc_cfg", alloc_cfg, CFGF_NONE),
  CFG_SEC("exec_cfg", exec_cfg, CFGF_NONE),
  CFG_SEC("commit_cfg", commit_cfg, CFGF_NONE),
  CFG_END()
};

cfg_opt_t fsb_cfg[] {
  CFG_INT("width", 4, CFGF_NONE),
  CFG_BOOL("ddr", cfg_false, CFGF_NONE),
  CFG_FLOAT("clock", 100.0, CFGF_NONE),
  CFG_BOOL("magic", cfg_false, CFGF_NONE),
  CFG_END()
};

cfg_opt_t dram_cfg[] {
  CFG_STR("memory_controller_config", "simple:4:1", CFGF_NONE),
  CFG_STR("dram_config", "simplescalar:80", CFGF_NONE),
  CFG_END()
};

cfg_opt_t dvfs_cfg[] {
  CFG_STR("config", "none", CFGF_NONE),
  CFG_INT("interval", 0, CFGF_NONE),
  CFG_END()
};

cfg_opt_t scheduler_cfg[] {
  CFG_INT("scheduler_tick", 0, CFGF_NONE),
  CFG_STR("allocator", "gang", CFGF_NONE),
  CFG_STR("allocator_opt_target", "throughput", CFGF_NONE),
  CFG_STR("speedup_model", "linear", CFGF_NONE),
  CFG_END()
};

cfg_opt_t uncore_cfg[] {
  CFG_SEC("llccache_cfg", llccache_cfg, CFGF_TITLE),
  CFG_SEC("fsb_cfg", fsb_cfg, CFGF_NONE),
  CFG_SEC("dram_cfg", dram_cfg, CFGF_NONE),
  CFG_SEC("dvfs_cfg", dvfs_cfg, CFGF_NONE),
  CFG_SEC("scheduler_cfg", scheduler_cfg, CFGF_NONE),
  CFG_END()
};

// Global view of all configuration options.
cfg_opt_t top_level_cfg[] {
  CFG_SEC("system_cfg", system_cfg, CFGF_NONE),
  CFG_SEC("core_cfg", core_cfg, CFGF_NONE),
  CFG_SEC("uncore_cfg", uncore_cfg, CFGF_NONE),
  CFG_END()
};

#pragma GCC diagnostic pop

