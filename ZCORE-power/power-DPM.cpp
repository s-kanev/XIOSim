/* power-DPM.cpp - Power proxy for OoO core */


#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(power_opt_string,"DPM"))
    return new core_power_DPM_t(core);
#else

class core_power_DPM_t : public core_power_t {

  public:
  core_power_DPM_t(struct core_t *_core);

  void translate_params(system_core *core_params);
  void translate_stats(system_core* core_stats);
};

core_power_DPM_t::core_power_DPM_t(struct core_t *_core):
  core_power_t(_core)
{
}

void core_power_DPM_t::translate_params(system_core *core_params)
{
  core_power_t::translate_params(core_params);

  core_params->machine_type = 1; // In-order
  core_params->number_hardware_threads = 2;
  core_params->number_instruction_fetch_ports = 2;
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

void core_power_DPM_t::translate_stats(system_core *core_stats)
{
  core_power_t::translate_stats(core_stats);
}
#endif
