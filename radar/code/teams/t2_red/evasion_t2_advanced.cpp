// evasion_t2_advanced.cpp -- sim-only composition of advanced T2 evasions.

#include "t2_red/evasion_t2_advanced.hpp"

#include <sstream>

namespace t2_red {

AdvancedT2Report AdvancedT2Evasion::max_kernel_stealth(sim::World& world,
                                                         KernelRadar& radar,
                                                         KernelPath path) {
  AdvancedT2Report report;
  report.driver_loaded = radar.bring_up(path);
  if (!report.driver_loaded) {
    report.detail = "advanced_t2 bring_up_failed";
    world.note("t2 AdvancedT2Evasion " + report.detail);
    return report;
  }

  radar.strip_callbacks();
  radar.enable_dkom_token_steal();
  radar.enable_physmem_direct_map();
  radar.enable_wfp_ndis_filter();
  radar.enable_pool_tag_hide();
  radar.enable_etw_ti_blind();
  radar.enable_instrumentation_callback();
  if (path == KernelPath::AcpiSmbios) {
    radar.enable_acpi_pm_read(1);
  } else if (path == KernelPath::Hypercall) {
    radar.enable_hypercall_read();
  }

  const auto& radar_report = radar.last_report();
  report.callbacks_stripped = radar_report.callback_stripped;
  report.dkom_stolen = radar_report.dkom_stolen;
  report.physmem_mapped = radar_report.physmem_mapped;
  report.acpi_read = radar_report.acpi_read;
  report.hypercall_read = radar_report.hypercall_read;
  report.pool_hidden = radar_report.pool_hidden;
  report.wfp_installed = radar_report.wfp_installed;
  report.etw_ti_blind = radar_report.etw_ti_blind;
  report.instr_callback_stealth = radar_report.instr_callback;
  report.no_game_handle = !radar.has_game_handle();

  if (report.driver_loaded) report.active_techniques.emplace_back("driver_load");
  if (report.callbacks_stripped) report.active_techniques.emplace_back("callback_strip");
  if (report.dkom_stolen) report.active_techniques.emplace_back("dkom_token_steal");
  if (report.physmem_mapped) report.active_techniques.emplace_back("physmem_direct_map");
  if (report.acpi_read) report.active_techniques.emplace_back("acpi_pm_read");
  if (report.hypercall_read) report.active_techniques.emplace_back("hypercall_read");
  if (report.pool_hidden) report.active_techniques.emplace_back("pool_tag_hide");
  if (report.wfp_installed) report.active_techniques.emplace_back("wfp_ndis_filter");
  if (report.etw_ti_blind) report.active_techniques.emplace_back("etw_ti_blind");
  if (report.instr_callback_stealth) {
    report.active_techniques.emplace_back("instrumentation_callback");
  }
  report.technique_count = static_cast<int>(report.active_techniques.size());

  if (!radar.driver_sha().empty()) report.remaining_scars.emplace_back("driver_hash");
  if (!radar.device().empty()) report.remaining_scars.emplace_back("device_name");
  if (!world.services.empty()) report.remaining_scars.emplace_back("service");
  if (!world.ac_callback_present) report.remaining_scars.emplace_back("callback_absence");

  std::ostringstream detail;
  detail << "advanced_t2 techniques=" << report.technique_count
         << " scars=" << report.remaining_scars.size()
         << " no_handle=" << (report.no_game_handle ? 1 : 0);
  report.detail = detail.str();
  world.note("t2 AdvancedT2Evasion " + report.detail);
  return report;
}

AdvancedT2Report AdvancedT2Evasion::deep_kernel_stealth(sim::World& world,
                                                          KernelRadar& radar) {
  AdvancedT2Report report;
  report.driver_loaded = radar.bring_up(KernelPath::Byovd);
  if (!report.driver_loaded) {
    report.detail = "deep_kernel_stealth bring_up_failed";
    world.note("t2 AdvancedT2Evasion " + report.detail);
    return report;
  }

  radar.strip_callbacks();
  radar.enable_dkom_token_steal();
  radar.enable_physmem_direct_map();
  radar.enable_acpi_pm_read(1);
  radar.enable_hypercall_read();
  radar.enable_pool_tag_hide();
  radar.enable_wfp_ndis_filter();
  radar.enable_etw_ti_blind();
  radar.enable_instrumentation_callback();
  radar.enable_hal_heap_exploit();
  radar.enable_null_ptr_deref_exploit();
  radar.enable_dpc_execution(1);
  radar.enable_module_shadowing("LabVulnDrv.sys");

  const auto& radar_report = radar.last_report();
  report.callbacks_stripped = radar_report.callback_stripped;
  report.dkom_stolen = radar_report.dkom_stolen;
  report.physmem_mapped = radar_report.physmem_mapped;
  report.acpi_read = radar_report.acpi_read;
  report.hypercall_read = radar_report.hypercall_read;
  report.pool_hidden = radar_report.pool_hidden;
  report.wfp_installed = radar_report.wfp_installed;
  report.etw_ti_blind = radar_report.etw_ti_blind;
  report.instr_callback_stealth = radar_report.instr_callback;
  report.hal_heap_exploit = radar_report.hal_heap_exploit;
  report.null_ptr_deref = radar_report.null_ptr_deref;
  report.dpc_execution = radar_report.dpc_execution;
  report.dpc_queue_count = radar_report.dpc_queue_count;
  report.module_shadowing = radar_report.module_shadowing;
  report.no_game_handle = !radar.has_game_handle();

  if (report.driver_loaded) report.active_techniques.emplace_back("driver_load");
  if (report.callbacks_stripped) report.active_techniques.emplace_back("callback_strip");
  if (report.dkom_stolen) report.active_techniques.emplace_back("dkom_token_steal");
  if (report.physmem_mapped) report.active_techniques.emplace_back("physmem_direct_map");
  if (report.acpi_read) report.active_techniques.emplace_back("acpi_pm_read");
  if (report.hypercall_read) report.active_techniques.emplace_back("hypercall_read");
  if (report.pool_hidden) report.active_techniques.emplace_back("pool_tag_hide");
  if (report.wfp_installed) report.active_techniques.emplace_back("wfp_ndis_filter");
  if (report.etw_ti_blind) report.active_techniques.emplace_back("etw_ti_blind");
  if (report.instr_callback_stealth) {
    report.active_techniques.emplace_back("instrumentation_callback");
  }
  if (report.hal_heap_exploit) report.active_techniques.emplace_back("hal_heap_exploit");
  if (report.null_ptr_deref) report.active_techniques.emplace_back("null_ptr_deref");
  if (report.dpc_execution) report.active_techniques.emplace_back("dpc_execution");
  if (report.module_shadowing) report.active_techniques.emplace_back("module_shadowing");
  report.technique_count = static_cast<int>(report.active_techniques.size());

  if (!radar.driver_sha().empty()) report.remaining_scars.emplace_back("driver_hash");
  if (!radar.device().empty()) report.remaining_scars.emplace_back("device_name");
  if (!world.services.empty()) report.remaining_scars.emplace_back("service");
  if (!world.ac_callback_present) report.remaining_scars.emplace_back("callback_absence");

  std::ostringstream detail;
  detail << "deep_kernel_stealth techniques=" << report.technique_count
         << " scars=" << report.remaining_scars.size()
         << " dpc_count=" << report.dpc_queue_count
         << " no_handle=" << (report.no_game_handle ? 1 : 0);
  report.detail = detail.str();
  world.note("t2 AdvancedT2Evasion " + report.detail);
  return report;
}

}  // namespace t2_red
