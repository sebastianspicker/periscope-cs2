#pragma once

// Simulated CS2 diagnostic collection sequence. Blue collects the telemetry;
// red can mutate only the simulation state before collection.

#include "cs2/ac_evasion.hpp"
#include "cs2/diagnostic_sensors.hpp"
#include "cs2/diagnostic_stack_analysis.hpp"
#include "cs2/offsets.hpp"
#include "sim/world.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace cs2 {

struct DiagnosticTelemetrySnapshot {
  sensors::ModuleSnapshotReport module_snapshot;
  sensors::PETimestampReport pe_timestamps;
  sensors::ThreadCaptureReport thread_capture;
  sensors::CpuidVmReport cpuid_vm;
  sensors::DebugMonitorReport debug_monitor;
  int total_files = 0;
  int files_need_check = 0;
  int files_completed = 0;
  bool has_violations = false;

  sensors::ConVarIntegrityReport convar_integrity;
  sensors::VmtIntegrityReport vmt_integrity;
  sensors::VmtIntegrityExtendedReport vmt_integrity_extended;
  sensors::FocusCorrelationReport focus_correlation;
  sensors::ModuleAnalysisReport module_analysis;
  sensors::ExceptionMonitorReport exception_monitor;
  sensors::CounterStrafeReport counter_strafe;

  // Pattern / behavioral sensors (Message 385 + call-graph telemetry).
  sensors::CallGraphReport call_graph;
  sensors::FrequencyReport pattern_frequency;
  sensors::MessageFrequencyReport message_frequency;
  sensors::EventListenerReport event_listeners;
  sensors::SubtickAnalysisReport subtick;
  bool stack_contains_cheat_frames = false;
  bool self_patching_active = false;

  std::uint32_t pid = 0;
  std::uint32_t os_type = 0;
  std::uint64_t system_time = 0;
  bool is_active = false;
  bool has_focus = false;

  bool diagnostics_sent = false;
  int messages_collected = 0;
  std::string detail;
};

class DiagnosticOrchestrator {
 public:
  explicit DiagnosticOrchestrator(sim::World& world);

  DiagnosticTelemetrySnapshot run_full_diagnostics();
  DiagnosticTelemetrySnapshot run_system_diagnostics();
  DiagnosticTelemetrySnapshot run_convar_check();
  DiagnosticTelemetrySnapshot run_vmt_module_analysis();
  DiagnosticTelemetrySnapshot run_pattern_analysis();
  sensors::MemoryDumpReport run_memory_dump(std::uint64_t address,
                                            std::size_t size);
  evasion::AntiDiagnosticReport apply_evasions(std::uint32_t cheat_pid);
  bool is_secure_client() const;

  // Optional cheat pid for stack analysis; 0 skips cheat-frame walk.
  void set_suspect_pid(std::uint32_t pid) { suspect_pid_ = pid; }
  std::uint32_t suspect_pid() const { return suspect_pid_; }

 private:
  sim::World& world_;
  std::uint32_t suspect_pid_ = 0;
  sensors::ModuleSnapshotSensor module_sensor_;
  sensors::PETimestampSensor pe_sensor_;
  sensors::ThreadCaptureSensor thread_sensor_;
  sensors::ConVarIntegritySensor convar_sensor_;
  sensors::VmtIntegritySensor vmt_sensor_;
  sensors::VmtExtendedSensor vmt_extended_sensor_;
  sensors::FocusCorrelationSensor focus_sensor_;
  sensors::ModuleAnalysisSensor module_analysis_sensor_;
  sensors::ExceptionMonitorSensor exception_sensor_;
  sensors::CounterStrafeSensor cs_sensor_;
  sensors::CpuidVmSensor cpuid_sensor_;
  sensors::DebugMonitorSensor debug_sensor_;
  sensors::MemoryDumpSensor dump_sensor_;
  sensors::CallGraphSensor call_graph_sensor_;
  sensors::PatternFrequencySensor pattern_frequency_sensor_;
  sensors::MessageFrequencySensor message_frequency_sensor_;
  sensors::EventListenerSensor event_listener_sensor_;
  sensors::SubtickAnalysisSensor subtick_sensor_;
  sensors::StackAnalysisSensor stack_sensor_;

  void record_evidence(bool detected);
  void populate_system_state(DiagnosticTelemetrySnapshot& snapshot) const;
};

}  // namespace cs2
