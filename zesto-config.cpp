/* Read Zesto configuration file and store settings in the knobs structure.
 * See the header for any relevant documentation.
 *
 * Author: Sam Xi
 */

#include <cmath>
#include <string>
#include <sstream>

#include "pintool/ezOptionParser_clean.hpp"

#include "machine.h"  // Make sure this won't interfere with Boost.
#include "sim.h"
#include "confuse.h"
#include "zesto-config.h"
#include "zesto-structs.h"

void store_fetch_options(cfg_t *fetch_opt, core_knobs_t *knobs) {
  cfg_t *icache_opt = cfg_getsec(fetch_opt, "icache_cfg");
  cfg_t *prefetch_opt = cfg_getsec(icache_opt, "iprefetch_cfg");
  cfg_t *itlb_opt = cfg_getsec(icache_opt, "itlb_cfg");
  cfg_t *branch_pred_opt = cfg_getsec(fetch_opt, "branch_pred_cfg");
  cfg_t *byte_queue_opt = cfg_getsec(fetch_opt, "byte_queue_cfg");
  cfg_t *predecode_opt = cfg_getsec(fetch_opt, "predecode_cfg");

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
  knobs->memory.IL1_controller_opt_str =
      cfg_getstr(icache_opt, "coherency_controller");
  knobs->memory.IL1_magic_hit_rate = cfg_getfloat(icache_opt, "magic_hit_rate");
  store_str_list(prefetch_opt, "config", knobs->memory.IL1PF_opt_str,
                 &knobs->memory.IL1_num_PF, MAX_PREFETCHERS);
  knobs->memory.IL1_PFFsize = cfg_getint(prefetch_opt, "fifosize");
  knobs->memory.IL1_PF_buffer_size = cfg_getint(prefetch_opt, "buffer");
  knobs->memory.IL1_PF_filter_size = cfg_getint(prefetch_opt, "filter");
  knobs->memory.IL1_PF_filter_reset = cfg_getint(prefetch_opt, "filter_reset");
  knobs->memory.IL1_PFthresh = cfg_getint(prefetch_opt, "threshold");
  knobs->memory.IL1_PFmax =
      cfg_getint( prefetch_opt, "max_outstanding_requests");
  knobs->memory.IL1_low_watermark = cfg_getfloat(prefetch_opt, "watermark_min");
  knobs->memory.IL1_high_watermark =
      cfg_getfloat(prefetch_opt, "watermark_max");
  knobs->memory.IL1_WMinterval = cfg_getint(prefetch_opt, "watermark_sampling_interval");
  knobs->memory.ITLB_opt_str = cfg_getstr(itlb_opt, "config");
  knobs->memory.ITLB_controller_opt_str =
      cfg_getstr(itlb_opt, "coherency_controller");
}

void store_decode_options(cfg_t *decode_opt, core_knobs_t *knobs) {
  cfg_t *uop_fusion_opt = cfg_getsec(decode_opt, "uop_fusion_cfg");

  knobs->decode.depth = cfg_getint(decode_opt, "depth");
  knobs->decode.width = cfg_getint(decode_opt, "width");
  knobs->decode.target_stage = cfg_getint(decode_opt, "branch_agen_stage");
  knobs->decode.branch_decode_limit =
      cfg_getint(decode_opt, "branch_decode_limit");
  store_int_list(decode_opt,
                 "decoder_max_uops",
                 knobs->decode.decoders,
                 &knobs->decode.num_decoder_specs,
                 MAX_DECODE_WIDTH);
  knobs->decode.MS_latency = cfg_getint(decode_opt, "ucode_sequencer_latency");
  knobs->decode.uopQ_size = cfg_getint(decode_opt, "uop_queue_size");
  knobs->decode.fusion_mode.LOAD_OP = cfg_getbool(uop_fusion_opt, "load_comp_op");
  knobs->decode.fusion_mode.FP_LOAD_OP = cfg_getbool(uop_fusion_opt, "fpload_comp_op");
  knobs->decode.fusion_mode.STA_STD = cfg_getbool(uop_fusion_opt, "sta_std");
  knobs->decode.fusion_mode.LOAD_OP_ST = cfg_getbool(uop_fusion_opt, "load_op_store");
}

void store_alloc_options(cfg_t *alloc_opt, core_knobs_t *knobs) {
  knobs->alloc.depth = cfg_getint(alloc_opt, "depth");
  knobs->alloc.width = cfg_getint(alloc_opt, "width");
  knobs->alloc.drain_flush = cfg_getbool(alloc_opt, "use_drain_flush");
}

void store_int_list(cfg_t* cfg,
                    const char* attr_name,
                    int* target,
                    int* num_entries,
                    int max_entries) {
  *num_entries = fmin(max_entries, cfg_size(cfg, attr_name));
  for (int i = 0; i < *num_entries; i++) {
    target[i] = cfg_getnint(cfg, attr_name, i);
  }
}

void store_str_list(cfg_t* cfg,
                    const char* attr_name,
                    const char* target[],
                    int* num_entries,
                    int max_entries) {
  *num_entries = fmin(max_entries, cfg_size(cfg, attr_name));
  for (int i = 0; i < *num_entries; i++) {
    target[i] = cfg_getnstr(cfg, attr_name, i);
  }
}

void store_execution_unit_options(cfg_t *exec_opt,
                                  const char* exeu_name,
                                  fu_class fu_type,
                                  core_knobs_t *knobs) {
  cfg_t *exeu_opt = cfg_gettsec(exec_opt, "exeu", exeu_name);
  // If this execution unit has not been declared, it doesn't exist in the
  // target system, so we should not set any defaults.
  if (!exeu_opt)
    return;
  store_int_list(exeu_opt,
                 "port_binding",
                 knobs->exec.fu_bindings[fu_type],
                 &knobs->exec.port_binding[fu_type].num_FUs,
                 MAX_EXEC_WIDTH);
  knobs->exec.latency[fu_type] = cfg_getint(exeu_opt, "latency");
  knobs->exec.issue_rate[fu_type] = cfg_getint(exeu_opt, "rate");
}

void store_exec_stage_options(cfg_t *exec_opt, core_knobs_t *knobs) {
  cfg_t *dcache_opt = cfg_getsec(exec_opt, "dcache_cfg");
  cfg_t *l2_opt = cfg_getsec(exec_opt, "l2cache_cfg");
  cfg_t *dtlb_opt = cfg_getsec(dcache_opt, "dtlb_cfg");
  cfg_t *d2tlb_opt = cfg_getsec(dcache_opt, "d2tlb_cfg");
  cfg_t *dpf_opt = cfg_getsec(dcache_opt, "dprefetch_cfg");
  cfg_t *l2pf_opt = cfg_getsec(l2_opt, "l2prefetch_cfg");
  cfg_t *repeater_opt = cfg_getsec(exec_opt, "repeater_cfg");

  knobs->exec.RS_size = cfg_getint(exec_opt, "rs_size");
  knobs->exec.LDQ_size = cfg_getint(exec_opt, "loadq_size");
  knobs->exec.STQ_size = cfg_getint(exec_opt, "storeq_size");
  knobs->exec.num_exec_ports = cfg_getint(exec_opt, "width");
  knobs->exec.payload_depth = cfg_getint(exec_opt, "payload_depth");
  knobs->exec.tornado_breaker = cfg_getbool(exec_opt, "enable_tornado_breaker");
  knobs->exec.throttle_partial =
      cfg_getbool(exec_opt, "enable_partial_throttle");
  knobs->exec.fp_penalty = cfg_getint(exec_opt, "fp_forward_penalty");
  knobs->exec.memdep_opt_str = cfg_getstr(exec_opt, "mem_dep_pred_config");

  knobs->memory.DL1_opt_str = cfg_getstr(dcache_opt, "config");
  knobs->memory.DL1_MSHR_cmd = cfg_getstr(dcache_opt, "mshr_cmd");
  knobs->memory.DL1_controller_opt_str =
      cfg_getstr(dcache_opt, "coherency_controller");
  knobs->memory.DL1_magic_hit_rate = cfg_getfloat(dcache_opt, "magic_hit_rate");
  knobs->memory.DL2_opt_str = cfg_getstr(l2_opt, "config");
  knobs->memory.DL2_MSHR_cmd = cfg_getstr(l2_opt, "mshr_cmd");
  knobs->memory.DL2_controller_opt_str =
      cfg_getstr(l2_opt, "coherency_controller");
  knobs->memory.DL2_magic_hit_rate = cfg_getfloat(l2_opt, "magic_hit_rate");

  store_str_list(dpf_opt, "config", knobs->memory.DL1PF_opt_str,
                 &knobs->memory.DL1_num_PF, MAX_PREFETCHERS);
  knobs->memory.DL1_PFFsize = cfg_getint(dpf_opt, "fifosize");
  knobs->memory.DL1_PF_buffer_size = cfg_getint(dpf_opt, "buffer");
  knobs->memory.DL1_PF_filter_size = cfg_getint(dpf_opt, "filter");
  knobs->memory.DL1_PF_filter_reset = cfg_getint(dpf_opt, "filter_reset");
  knobs->memory.DL1_PFthresh = cfg_getint(dpf_opt, "threshold");
  knobs->memory.DL1_PFmax =
      cfg_getint( dpf_opt, "max_outstanding_requests");
  knobs->memory.DL1_low_watermark = cfg_getfloat(dpf_opt, "watermark_min");
  knobs->memory.DL1_high_watermark =
      cfg_getfloat(dpf_opt, "watermark_max");
  knobs->memory.DL1_WMinterval = cfg_getint(dpf_opt, "watermark_sampling_interval");

  store_str_list(l2pf_opt, "config", knobs->memory.DL2PF_opt_str,
                 &knobs->memory.DL2_num_PF, MAX_PREFETCHERS);
  knobs->memory.DL2_PFFsize = cfg_getint(l2pf_opt, "fifosize");
  knobs->memory.DL2_PF_buffer_size = cfg_getint(l2pf_opt, "buffer");
  knobs->memory.DL2_PF_filter_size = cfg_getint(l2pf_opt, "filter");
  knobs->memory.DL2_PF_filter_reset = cfg_getint(l2pf_opt, "filter_reset");
  knobs->memory.DL2_PFthresh = cfg_getint(l2pf_opt, "threshold");
  knobs->memory.DL2_PFmax =
      cfg_getint( l2pf_opt, "max_outstanding_requests");
  knobs->memory.DL2_low_watermark = cfg_getfloat(l2pf_opt, "watermark_min");
  knobs->memory.DL2_high_watermark =
      cfg_getfloat(l2pf_opt, "watermark_max");
  knobs->memory.DL2_WMinterval = cfg_getint(l2pf_opt, "watermark_sampling_interval");

  knobs->memory.DTLB_opt_str = cfg_getstr(dtlb_opt, "config");
  knobs->memory.DTLB_controller_opt_str =
      cfg_getstr(dtlb_opt, "coherency_controller");
  knobs->memory.DTLB2_opt_str = cfg_getstr(d2tlb_opt, "config");
  knobs->memory.DTLB2_controller_opt_str =
      cfg_getstr(d2tlb_opt, "coherency_controller");

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

void store_commit_options(cfg_t *commit_opt, core_knobs_t* knobs) {
  knobs->commit.ROB_size = cfg_getint(commit_opt, "rob_size");
  knobs->commit.width = cfg_getint(commit_opt, "commit_width");
  knobs->commit.branch_limit = cfg_getint(commit_opt, "commit_branches");
  knobs->commit.pre_commit_depth = cfg_getint(commit_opt, "precommit_depth");
}

void store_core_options(cfg_t *core_opt, core_knobs_t *knobs) {
  cfg_t *fetch_opt = cfg_getsec(core_opt, "fetch_cfg");
  cfg_t *decode_opt = cfg_getsec(core_opt, "decode_cfg");
  cfg_t *alloc_opt = cfg_getsec(core_opt, "alloc_cfg");
  cfg_t *exec_opt = cfg_getsec(core_opt, "exec_cfg");
  cfg_t *commit_opt = cfg_getsec(core_opt, "commit_cfg");
  knobs->default_cpu_speed = cfg_getfloat(core_opt, "core_clock");
  store_fetch_options(fetch_opt, knobs);
  store_decode_options(decode_opt, knobs);
  store_alloc_options(alloc_opt, knobs);
  store_exec_stage_options(exec_opt, knobs);
  store_commit_options(commit_opt, knobs);
}

void store_uncore_options(cfg_t *uncore_opt, core_knobs_t *knobs) {
  cfg_t* llccache_opt = cfg_getsec(uncore_opt, "llccache_cfg");
  cfg_t* llcprefetch_opt = cfg_getsec(llccache_opt, "llcprefetch_cfg");
  cfg_t* fsb_opt = cfg_getsec(uncore_opt, "fsb_cfg");
  cfg_t* dram_opt = cfg_getsec(uncore_opt, "dram_cfg");
  cfg_t* dvfs_opt = cfg_getsec(uncore_opt, "dvfs_cfg");
  cfg_t* scheduler_opt = cfg_getsec(uncore_opt, "scheduler_cfg");

  LLC_opt_str = cfg_getstr(llccache_opt, "config");
  LLC_MSHR_cmd = cfg_getstr(llccache_opt, "mshr_cmd");
  LLC_speed = cfg_getfloat(llccache_opt, "clock");
  LLC_controller_str = cfg_getstr(llccache_opt, "coherency_controller");
  LLC_magic_hit_rate = cfg_getfloat(llccache_opt, "magic_hit_rate");
  store_str_list(llcprefetch_opt, "config", LLC_PF_opt_str,
                 &LLC_num_PF, MAX_PREFETCHERS);
  LLC_PFFsize = cfg_getint(llcprefetch_opt, "fifosize");
  LLC_PF_buffer_size = cfg_getint(llcprefetch_opt, "buffer");
  LLC_PF_filter_size = cfg_getint(llcprefetch_opt, "filter");
  LLC_PF_filter_reset = cfg_getint(llcprefetch_opt, "filter_reset");
  LLC_PFthresh = cfg_getint(llcprefetch_opt, "threshold");
  LLC_PFmax = cfg_getint(llcprefetch_opt, "max_outstanding_requests");
  LLC_low_watermark = cfg_getfloat(llcprefetch_opt, "watermark_min");
  LLC_high_watermark = cfg_getfloat(llcprefetch_opt, "watermark_max");
  LLC_WMinterval = cfg_getint(llcprefetch_opt, "watermark_sampling_interval");

  fsb_width = cfg_getint(fsb_opt, "width");
  fsb_DDR = cfg_getbool(fsb_opt, "ddr");
  fsb_speed = cfg_getfloat(fsb_opt, "clock");
  fsb_magic = cfg_getbool(fsb_opt, "magic");

  MC_opt_string = cfg_getstr(dram_opt, "memory_controller_config");
  dram_opt_string = cfg_getstr(dram_opt, "dram_config");

  knobs->scheduler_tick = cfg_getint(scheduler_opt, "scheduler_tick");
  knobs->allocator = cfg_getstr(scheduler_opt, "allocator");
  knobs->allocator_opt_target =
      cfg_getstr(scheduler_opt, "allocator_opt_target");
  knobs->speedup_model = cfg_getstr(scheduler_opt, "speedup_model");

  knobs->dvfs_opt_str = cfg_getstr(dvfs_opt, "config");
  knobs->dvfs_interval = cfg_getint(dvfs_opt, "interval");
}

void store_system_options(cfg_t *system_opt, core_knobs_t *knobs) {
#ifdef DEBUG
  debugging = cfg_getbool(system_opt, "debug");
#endif
  assert_spin = cfg_getbool(system_opt, "assert_spin");
  rand_seed = cfg_getint(system_opt, "seed");
  num_cores = cfg_getint(system_opt, "num_cores");
  if((num_cores < 1) || (num_cores > MAX_CORES))
    fatal("-cores must be between 1 and %d (inclusive)", MAX_CORES);
  heartbeat_frequency = cfg_getint(system_opt, "heartbeat_interval");
  knobs->model = cfg_getstr(system_opt, "pipeline_model");
  ztrace_filename = cfg_getstr(system_opt, "ztrace_file_prefix");
  knobs->power.compute = cfg_getbool(system_opt, "simulate_power");
  knobs->power.rtp_interval = cfg_getint(system_opt, "power_rtp_interval");
  knobs->power.rtp_filename = cfg_getstr(system_opt, "power_rtp_file");
  sim_simout = cfg_getstr(system_opt, "output_redir");
}

// TODO(skanev): No need for reconstructing argc, argv. Move to "one set of
// flags to rule them all" in timing_sim
int read_config_file(int argc, const char* argv[], core_knobs_t *knobs) {
  ez::ezOptionParser opts;
  opts.overview = "XIOSim Zesto options";
  opts.syntax = "XXX";
  opts.add("", 1, 1, 0, "Simulator config file", "-config");
  opts.parse(argc, argv);

  std::string cfg_file;
  opts.get("-config")->getString(cfg_file);

  all_opts = cfg_init(top_level_cfg, CFGF_NOCASE);
  int ret = cfg_parse(all_opts, cfg_file.c_str());
  if (ret == CFG_FILE_ERROR) {
    std::stringstream err;
    err << "Failed to open configuration file " << cfg_file;
    perror(err.str().c_str());
    return 1;
  } else if (ret == CFG_PARSE_ERROR) {
    fprintf(stderr, "Parsing error.\n");
    return 2;
  }

  cfg_t *system_opt = cfg_getsec(all_opts, "system_cfg");
  cfg_t *core_opt = cfg_getsec(all_opts, "core_cfg");
  cfg_t *uncore_opt = cfg_getsec(all_opts, "uncore_cfg");

  store_system_options(system_opt, knobs);
  store_core_options(core_opt, knobs);
  store_uncore_options(uncore_opt, knobs);
  return 0;
}
