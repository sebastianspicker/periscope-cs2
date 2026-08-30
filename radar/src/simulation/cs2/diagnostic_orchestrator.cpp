#include "cs2/diagnostic_orchestrator.hpp"

#include <utility>

namespace cs2 {

DiagnosticOrchestrator::DiagnosticOrchestrator(sim::World& world)
    : world_(world),
      module_sensor_(world),
      pe_sensor_(world),
      thread_sensor_(world),
      convar_sensor_(world),
      vmt_sensor_(world),
      vmt_extended_sensor_(world),
      focus_sensor_(world),
      module_analysis_sensor_(world),
      exception_sensor_(world),
      cs_sensor_(world),
      cpuid_sensor_(world),
      debug_sensor_(world),
      dump_sensor_(world),
      call_graph_sensor_(world),
      pattern_frequency_sensor_(world),
      message_frequency_sensor_(world),
      event_listener_sensor_(world),
      subtick_sensor_(world),
      stack_sensor_(world) {}

void DiagnosticOrchestrator::populate_system_state(
    DiagnosticTelemetrySnapshot& snapshot) const {
  snapshot.pid = world_.game_pid();
  snapshot.is_active = world_.diagnostic_system_active;
  snapshot.has_focus = world_.match_active || world_.diagnostic_state.has_focus;
  snapshot.os_type = 2;  // Windows educational constant
  snapshot.system_time = static_cast<std::uint64_t>(world_.lab_match_tick);
  snapshot.total_files =
      static_cast<int>(world_.diagnostic_state.loaded_modules.size());
  snapshot.files_need_check = 0;
  snapshot.files_completed = 0;
  for (const auto& module : world_.diagnostic_state.loaded_modules) {
    if (module.suspicious || !module.expected) {
      ++snapshot.files_need_check;
    } else {
      ++snapshot.files_completed;
    }
  }
}

void DiagnosticOrchestrator::record_evidence(bool detected) {
  if (!detected || world_.silent_exclusion_active) return;

  ++world_.evidence_count;
  if (world_.evidence_count < world_.evidence_threshold) return;

  world_.silent_exclusion_active = true;
  world_.client_allowed_on_secure = false;
  world_.client_allowed_on_secure_servers = false;
  world_.diagnostic_state.b_secure_allowed = false;
  world_.exclusion_tick = static_cast<std::uint64_t>(world_.lab_match_tick);
  world_.note("secure-session exclusion activated");
}

DiagnosticTelemetrySnapshot DiagnosticOrchestrator::run_system_diagnostics() {
  DiagnosticTelemetrySnapshot snapshot;
  populate_system_state(snapshot);
  snapshot.module_snapshot = module_sensor_.scan();
  snapshot.pe_timestamps = pe_sensor_.verify();
  snapshot.thread_capture = thread_sensor_.capture();
  snapshot.cpuid_vm = cpuid_sensor_.detect();
  snapshot.debug_monitor = debug_sensor_.scan();
  snapshot.focus_correlation = focus_sensor_.analyze();
  populate_system_state(snapshot);
  snapshot.has_violations =
      snapshot.module_snapshot.has_suspicious_modules ||
      snapshot.pe_timestamps.any_mismatch ||
      snapshot.thread_capture.from_suspicious_module ||
      snapshot.thread_capture.from_rwx_memory ||
      snapshot.cpuid_vm.hypervisor_detected ||
      snapshot.debug_monitor.debugger_attached ||
      snapshot.focus_correlation.anomalous;
  snapshot.diagnostics_sent = true;
  snapshot.messages_collected = 1;
  world_.diag_module_snapshot_taken = true;
  world_.diag_pe_timestamps_sent = true;
  world_.diag_thread_captured = true;
  world_.diag_cpuid_vm_checked = true;
  world_.diag_debugger_checked = true;
  ++world_.diagnostic_response_count;
  world_.diagnostic_state.message_records.push_back(
      {kMsgDllStatus, static_cast<std::uint64_t>(world_.lab_match_tick), true});
  snapshot.detail = "Message 159 simulated system diagnostics collected.";
  world_.note("cs2 diagnostics: simulated Message 159 collected");
  return snapshot;
}

DiagnosticTelemetrySnapshot DiagnosticOrchestrator::run_convar_check() {
  DiagnosticTelemetrySnapshot snapshot;
  populate_system_state(snapshot);
  snapshot.convar_integrity = convar_sensor_.check();
  snapshot.has_violations = !snapshot.convar_integrity.integrity_ok;
  snapshot.diagnostics_sent = true;
  snapshot.messages_collected = 1;
  world_.diag_convar_checked = true;
  ++world_.diagnostic_response_count;
  world_.diagnostic_state.message_records.push_back(
      {kMsgUtilAction, static_cast<std::uint64_t>(world_.lab_match_tick), true});
  snapshot.detail = "Message 157 simulated ConVar integrity collected.";
  world_.note("cs2 diagnostics: simulated Message 157 collected");
  return snapshot;
}

DiagnosticTelemetrySnapshot DiagnosticOrchestrator::run_vmt_module_analysis() {
  DiagnosticTelemetrySnapshot snapshot;
  populate_system_state(snapshot);
  snapshot.vmt_integrity = vmt_sensor_.collect();
  snapshot.vmt_integrity_extended = vmt_extended_sensor_.check();
  snapshot.module_analysis = module_analysis_sensor_.analyze();
  snapshot.has_violations =
      snapshot.vmt_integrity.vmt_hooking_detected ||
      snapshot.vmt_integrity.interface_tamper_detected ||
      snapshot.vmt_integrity_extended.anomalous_vmt_found ||
      snapshot.vmt_integrity_extended.tampered_module_found ||
      snapshot.module_analysis.suspicious_module_found;
  snapshot.diagnostics_sent = true;
  snapshot.messages_collected = 1;
  world_.diag_vmt_collected = true;
  world_.diag_file_integrity_checked = true;
  ++world_.diagnostic_response_count;
  world_.diagnostic_state.message_records.push_back(
      {kMsgInventory, static_cast<std::uint64_t>(world_.lab_match_tick), true});
  snapshot.detail = "Message 160 simulated VMT and module analysis collected.";
  world_.note("cs2 diagnostics: simulated Message 160 collected");
  return snapshot;
}

DiagnosticTelemetrySnapshot DiagnosticOrchestrator::run_pattern_analysis() {
  DiagnosticTelemetrySnapshot snapshot;
  populate_system_state(snapshot);
  snapshot.call_graph = call_graph_sensor_.analyze();
  snapshot.pattern_frequency = pattern_frequency_sensor_.analyze();
  snapshot.message_frequency = message_frequency_sensor_.analyze();
  snapshot.event_listeners = event_listener_sensor_.analyze();
  snapshot.subtick = subtick_sensor_.analyze();
  snapshot.counter_strafe = cs_sensor_.analyze();

  if (suspect_pid_ != 0) {
    stack_sensor_.analyze_stack_for_cheat_frames(suspect_pid_);
  }
  snapshot.self_patching_active = stack_sensor_.is_self_patching_active();
  if (!world_.diagnostic_state.exceptions.empty()) {
    snapshot.stack_contains_cheat_frames =
        world_.diagnostic_state.exceptions.back().contains_cheat_frames;
  }

  snapshot.has_violations =
      snapshot.call_graph.anomalous_sequence ||
      snapshot.pattern_frequency.flagged_functions > 0 ||
      snapshot.message_frequency.unsolicited_anomaly ||
      snapshot.message_frequency.rate_anomalous ||
      snapshot.event_listeners.cheat_listener_detected ||
      snapshot.subtick.macro_suspected ||
      snapshot.subtick.automation_suspected ||
      snapshot.counter_strafe.macro_suspected ||
      snapshot.counter_strafe.automation_suspected ||
      snapshot.stack_contains_cheat_frames;
  snapshot.diagnostics_sent = true;
  snapshot.messages_collected = 1;
  world_.diag_counter_strafe_monitored = true;
  ++world_.diagnostic_response_count;
  world_.diagnostic_state.message_records.push_back(
      {kMsgCounterStrafe, static_cast<std::uint64_t>(world_.lab_match_tick),
       true});
  snapshot.detail =
      "Message 385 simulated pattern/subtick diagnostics collected.";
  world_.note("cs2 diagnostics: simulated Message 385 pattern analysis collected");
  return snapshot;
}

sensors::MemoryDumpReport DiagnosticOrchestrator::run_memory_dump(
    std::uint64_t address, std::size_t size) {
  sensors::MemoryDumpReport report = dump_sensor_.dump(address, size);
  world_.diag_memory_dump_performed = report.dump_performed;
  ++world_.diagnostic_response_count;
  world_.diagnostic_state.message_records.push_back(
      {kMsgMemoryDump, static_cast<std::uint64_t>(world_.lab_match_tick), true});
  world_.note("cs2 diagnostics: simulated Message 162 memory dump collected");
  return report;
}

DiagnosticTelemetrySnapshot DiagnosticOrchestrator::run_full_diagnostics() {
  DiagnosticTelemetrySnapshot snapshot = run_system_diagnostics();
  DiagnosticTelemetrySnapshot convar = run_convar_check();
  DiagnosticTelemetrySnapshot vmt_module = run_vmt_module_analysis();
  DiagnosticTelemetrySnapshot patterns = run_pattern_analysis();

  snapshot.convar_integrity = std::move(convar.convar_integrity);
  snapshot.vmt_integrity = std::move(vmt_module.vmt_integrity);
  snapshot.vmt_integrity_extended = std::move(vmt_module.vmt_integrity_extended);
  snapshot.module_analysis = std::move(vmt_module.module_analysis);
  snapshot.exception_monitor = exception_sensor_.monitor();
  snapshot.counter_strafe = std::move(patterns.counter_strafe);
  snapshot.call_graph = std::move(patterns.call_graph);
  snapshot.pattern_frequency = std::move(patterns.pattern_frequency);
  snapshot.message_frequency = std::move(patterns.message_frequency);
  snapshot.event_listeners = std::move(patterns.event_listeners);
  snapshot.subtick = std::move(patterns.subtick);
  snapshot.stack_contains_cheat_frames = patterns.stack_contains_cheat_frames;
  snapshot.self_patching_active = patterns.self_patching_active;

  snapshot.has_violations =
      snapshot.has_violations || convar.has_violations ||
      vmt_module.has_violations || patterns.has_violations ||
      snapshot.exception_monitor.exception_handler_active ||
      snapshot.exception_monitor.code_cave_detected ||
      snapshot.exception_monitor.self_patch_detected;
  record_evidence(snapshot.has_violations);
  snapshot.diagnostics_sent = true;
  snapshot.messages_collected = 5;
  world_.diag_exception_handler_active =
      snapshot.exception_monitor.exception_handler_active;
  world_.diag_counter_strafe_monitored = true;
  world_.diag_session_recorded = true;
  ++world_.diagnostic_response_count;
  snapshot.detail =
      "Messages 159, 157, 160, 162, and 385 simulated diagnostics collected.";
  world_.note("cs2 diagnostics: simulated full diagnostic sequence collected");
  return snapshot;
}

evasion::AntiDiagnosticReport DiagnosticOrchestrator::apply_evasions(
    std::uint32_t cheat_pid) {
  return evasion::apply_all_evasions(world_, cheat_pid);
}

bool DiagnosticOrchestrator::is_secure_client() const {
  return world_.client_allowed_on_secure_servers &&
         world_.client_allowed_on_secure &&
         world_.diagnostic_state.b_secure_allowed;
}

}  // namespace cs2
