/* power-IO-DPM.cpp - Power proxy for IO core */


#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(power_opt_string,"IO-DPM"))
    return new core_power_IO_DPM_t(core);
#else

extern class uncore_t * uncore;

class core_power_IO_DPM_t : public core_power_t {

  public:
  core_power_IO_DPM_t(struct core_t * _core);

  void translate_params(system_core *core_params, system_L2 *L2_params);
  void translate_stats(xiosim::stats::StatsDatabase* sdb,
                       system_core* core_params,
                       system_L2* L2_stats);
};

core_power_IO_DPM_t::core_power_IO_DPM_t(struct core_t * _core):
  core_power_t(_core)
{
}

void core_power_IO_DPM_t::translate_params(system_core *core_params, system_L2 *L2_params)
{
  core_power_t::translate_params(core_params, L2_params);

  core_params->machine_type = 1; // In-order
  core_params->number_hardware_threads = 2;
  core_params->number_instruction_fetch_ports = 1; // 2 because of SMT?
  core_params->fp_issue_width = 1;
  core_params->prediction_width = 1;
  core_params->pipelines_per_core[0] = 1;
  core_params->pipelines_per_core[1] = 1;
  core_params->pipeline_depth[0] = 16;
  core_params->pipeline_depth[1] = 16;

  core_params->instruction_window_scheme = 0; // PHYREG
  core_params->instruction_window_size = 0;
  core_params->archi_Regs_IRF_size = 16;
  core_params->archi_Regs_FRF_size = 32;
  core_params->phy_Regs_IRF_size = core_params->archi_Regs_IRF_size;
  core_params->phy_Regs_FRF_size = core_params->archi_Regs_FRF_size;
  core_params->rename_scheme = 2; //disabled
  core_params->register_windows_size = 0;
  strcpy(core_params->LSU_order, "inorder");
  core_params->memory_ports = 1;

}

void core_power_IO_DPM_t::translate_stats(xiosim::stats::StatsDatabase* sdb,
                                          system_core* core_stats,
                                          system_L2* L2_stats) {
  core_power_t::translate_stats(sdb, core_stats, L2_stats);

  core_stats->int_instructions = core_stats->total_instructions; // XXX: only used for inst window, for which we don't care in this pipe
  core_stats->fp_instructions = 0;

  core_stats->committed_int_instructions = 0;
  core_stats->committed_fp_instructions = 0;

  core_stats->ROB_reads = 0;
  core_stats->ROB_writes = 0;
  core_stats->rename_reads = 0;
  core_stats->rename_writes = 0;
  core_stats->fp_rename_reads = 0;
  core_stats->fp_rename_writes = 0;

  core_stats->inst_window_reads = 0;
  core_stats->inst_window_writes = 0;
  core_stats->inst_window_wakeup_accesses = 0;
  core_stats->fp_inst_window_reads = 0;
  core_stats->fp_inst_window_writes = 0;
  core_stats->fp_inst_window_wakeup_accesses = 0;

  core_stats->context_switches = 0;
}

#endif
