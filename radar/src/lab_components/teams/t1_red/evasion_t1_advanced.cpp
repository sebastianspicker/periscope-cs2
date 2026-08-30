// evasion_t1_advanced.cpp -- sim-only composition of advanced T1 evasions.

#include "t1_red/evasion_t1_advanced.hpp"

#include <sstream>

namespace t1_red {

AdvancedT1EvasionReport AdvancedT1Evasion::apply(int windows_build,
                                                   int spoof_depth) {
  AdvancedT1EvasionReport report;
  report.hooks_detected = cheat_.detect_and_evade_ntdll_hooks();
  const auto& hook_report = cheat_.last_report();
  report.clean_ntdll_copy = hook_report.used_clean_ntdll_copy;
  if (report.hooks_detected) {
    report.heavens_gate = cheat_.enable_heavens_gate();
  }
  report.dynamic_ssns = cheat_.resolve_dynamic_ssns(windows_build);
  report.enhanced_stack_spoof = cheat_.enable_enhanced_stack_spoof(spoof_depth);
  cheat_.apply_read_evasions(false, true);
  report.etw_blind = cheat_.last_report().etw_blind;
  report.sedebug = cheat_.enable_sedebug();
  report.threshold_evasion = cheat_.enable_threshold_evasion();
  const auto& cheat_report = cheat_.last_report();
  report.heavens_gate_stub_pid = cheat_report.heavens_gate_stub_pid;
  report.resolved_ssn_windows_build = cheat_report.resolved_ssn_windows_build;
  report.spoofed_call_depth = cheat_report.spoofed_call_depth;
  report.hooks_evaded_count = cheat_report.hooks_evaded_count;
  report.rop_chain_syscall = cheat_report.rop_chain_syscall;
  report.rop_gadget_count = cheat_report.rop_gadget_count;
  report.hw_breakpoint_evasion = cheat_report.hw_breakpoint_evasion;
  report.veh_cf_patched = cheat_report.veh_cf_patched;
  report.veh_handlers_modified = cheat_report.veh_handlers_modified;
  report.dynamic_import_resolution = cheat_report.dynamic_import_resolution;
  report.dynamic_import_count = cheat_report.dynamic_import_count;

  // Every technique deliberately leaves a World scar so the blue lesson can
  // correlate behavior even when a single usermode signal is blinded.
  if (report.clean_ntdll_copy) {
    report.remaining_scars.emplace_back("clean_ntdll_copy");
  }
  if (report.heavens_gate) {
    report.remaining_scars.emplace_back("wow64_heavens_gate_stub");
  }
  if (report.enhanced_stack_spoof) {
    report.remaining_scars.emplace_back("forged_call_stack");
  }
  if (report.dynamic_ssns) {
    report.remaining_scars.emplace_back("runtime_ssn_build_resolution");
  }
  if (report.sedebug) {
    report.remaining_scars.emplace_back("sedebug_privilege");
  }
  if (report.threshold_evasion) {
    report.remaining_scars.emplace_back("thread_hide_and_peb_spoof");
  }
  if (report.etw_blind) {
    report.remaining_scars.emplace_back("etw_disabled");
  }

  std::ostringstream detail;
  detail << "advanced_t1 hooks=" << (report.hooks_detected ? 1 : 0)
         << " gate=" << (report.heavens_gate ? 1 : 0)
         << " ssns=" << (report.dynamic_ssns ? 1 : 0)
         << " stack=" << (report.enhanced_stack_spoof ? 1 : 0)
         << " hooks_evaded=" << report.hooks_evaded_count
         << " scars=" << report.remaining_scars.size();
  report.detail = detail.str();
  cheat_.world().note("t1 AdvancedT1Evasion " + report.detail);
  return report;
}

// AdvancedT1Evasion::deep_stealth: Compose every T1 World-only scar in the
// prescribed lesson order. Counts are fixed, observable simulation parameters.
AdvancedT1EvasionReport AdvancedT1Evasion::deep_stealth() {
  AdvancedT1EvasionReport report;
  if (!cheat_.stage() || !cheat_.load_offsets()) {
    report.detail = cheat_.last_report().detail;
    return report;
  }

  report.hooks_detected = cheat_.detect_and_evade_ntdll_hooks();
  const auto& hook_report = cheat_.last_report();
  report.clean_ntdll_copy = hook_report.used_clean_ntdll_copy;
  report.hooks_evaded_count = hook_report.hooks_evaded_count;
  if (!cheat_.enable_heavens_gate() || !cheat_.enable_enhanced_stack_spoof(8) ||
      !cheat_.resolve_dynamic_ssns(22631) || !cheat_.enable_rop_chain_syscall(8) ||
      !cheat_.enable_hw_breakpoint_evasion() || !cheat_.enable_veh_cf_patch(2) ||
      !cheat_.enable_dynamic_import_resolution(12) || !cheat_.enable_sedebug() ||
      !cheat_.enable_threshold_evasion()) {
    report.detail = cheat_.last_report().detail;
    return report;
  }
  cheat_.apply_read_evasions(false, true);

  const auto& cheat_report = cheat_.last_report();
  report.heavens_gate = cheat_report.heavens_gate;
  report.heavens_gate_stub_pid = cheat_report.heavens_gate_stub_pid;
  report.dynamic_ssns = cheat_report.dynamic_ssn_resolved;
  report.resolved_ssn_windows_build = cheat_report.resolved_ssn_windows_build;
  report.enhanced_stack_spoof = cheat_report.enhanced_stack_spoof;
  report.spoofed_call_depth = cheat_report.spoofed_call_depth;
  report.etw_blind = cheat_report.etw_blind;
  report.sedebug = cheat_report.sedebug_enabled;
  report.threshold_evasion = cheat_report.threshold_evasion;
  report.rop_chain_syscall = cheat_report.rop_chain_syscall;
  report.rop_gadget_count = cheat_report.rop_gadget_count;
  report.hw_breakpoint_evasion = cheat_report.hw_breakpoint_evasion;
  report.veh_cf_patched = cheat_report.veh_cf_patched;
  report.veh_handlers_modified = cheat_report.veh_handlers_modified;
  report.dynamic_import_resolution = cheat_report.dynamic_import_resolution;
  report.dynamic_import_count = cheat_report.dynamic_import_count;

  if (report.clean_ntdll_copy) {
    report.remaining_scars.emplace_back("clean_ntdll_copy");
  }
  if (report.heavens_gate) {
    report.remaining_scars.emplace_back("wow64_heavens_gate_stub");
  }
  if (report.enhanced_stack_spoof) {
    report.remaining_scars.emplace_back("forged_call_stack");
  }
  if (report.dynamic_ssns) {
    report.remaining_scars.emplace_back("runtime_ssn_build_resolution");
  }
  if (report.rop_chain_syscall) {
    report.remaining_scars.emplace_back("rop_chain_syscall");
  }
  if (report.hw_breakpoint_evasion) {
    report.remaining_scars.emplace_back("debug_registers_cleared");
  }
  if (report.veh_cf_patched) {
    report.remaining_scars.emplace_back("veh_cf_patch");
  }
  if (report.dynamic_import_resolution) {
    report.remaining_scars.emplace_back("dynamic_import_resolution");
  }
  if (report.sedebug) {
    report.remaining_scars.emplace_back("sedebug_privilege");
  }
  if (report.threshold_evasion) {
    report.remaining_scars.emplace_back("thread_hide_and_peb_spoof");
  }
  if (report.etw_blind) {
    report.remaining_scars.emplace_back("etw_disabled");
  }

  std::ostringstream detail;
  detail << "deep_t1 hooks=" << (report.hooks_detected ? 1 : 0)
         << " stack_depth=" << report.spoofed_call_depth
         << " rop_gadgets=" << report.rop_gadget_count
         << " veh_handlers=" << report.veh_handlers_modified
         << " dynamic_imports=" << report.dynamic_import_count
         << " scars=" << report.remaining_scars.size();
  report.detail = detail.str();
  cheat_.world().note("t1 AdvancedT1Evasion " + report.detail);
  return report;
}

}  // namespace t1_red
