#pragma once

// Blue team diagnostic sensors — simulated CS2 built-in anti-cheat checks.
// Each sensor mirrors one of the real CS2 data collection mechanisms.
// All are sim-only, operating on World struct fields.

#include "cs2/diagnostics.hpp"
#include "sim/world.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cs2::sensors {

struct ModuleSnapshotReport {
  int total_modules = 0;
  int game_modules = 0;
  int system_modules = 0;
  int ac_modules = 0;
  int unknown_modules = 0;
  std::vector<DiagnosticModuleInfo> unknown_details;
  bool has_suspicious_modules = false;
  std::string detail;
};
class ModuleSnapshotSensor {
 public:
  explicit ModuleSnapshotSensor(sim::World& world);
  ModuleSnapshotReport scan();
  void capture_modules_from_world();

 private:
  sim::World& world_;
};

struct PETimestampReport {
  bool client_dll_valid = false;
  bool kernel32_valid = false;
  bool cs2_exe_valid = false;
  bool ntdll_valid = false;
  bool gameoverlay_valid = false;
  bool any_mismatch = false;
  int mismatched_count = 0;
  std::vector<std::string> mismatched_modules;
  std::string detail;
};
class PETimestampSensor {
 public:
  explicit PETimestampSensor(sim::World& world);
  PETimestampReport verify();

 private:
  bool check_timestamp(std::uint32_t actual, std::uint32_t expected,
                       const char* name, PETimestampReport& report);
  sim::World& world_;
};

struct ThreadCaptureReport {
  bool thread_captured = false;
  bool from_suspicious_module = false;
  bool from_rwx_memory = false;
  bool from_system_dll = false;
  bool signature_captured = false;
  std::uint32_t thread_id = 0;
  std::string detail;
};
class ThreadCaptureSensor {
 public:
  explicit ThreadCaptureSensor(sim::World& world);
  ThreadCaptureReport capture();

 private:
  sim::World& world_;
};

struct ConVarIntegrityReport {
  std::uint32_t crc_mode1 = 0;
  std::uint32_t crc_mode2 = 0;
  int total_convars = 0;
  int processed_mode1 = 0;
  int processed_mode2 = 0;
  bool integrity_ok = true;
  int tampered_count = 0;
  std::vector<std::string> tampered_convars;
  std::string detail;
};
class ConVarIntegritySensor {
 public:
  explicit ConVarIntegritySensor(sim::World& world);
  ConVarIntegrityReport check();

 private:
  std::uint32_t hash_djb2(const std::string& str) const;
  std::uint32_t compute_crc32(const std::uint8_t* data, std::size_t size) const;
  sim::World& world_;
};

struct VmtIntegrityReport {
  int interface_count = 0;
  int entity_vmt_count = 0;
  std::uint32_t crc_interfaces = 0;
  int anomalous_vmts = 0;
  bool vmt_hooking_detected = false;
  bool interface_tamper_detected = false;
  std::vector<DiagnosticVmtEntry> suspicious_vmts;
  std::string detail;
};
class VmtIntegritySensor {
 public:
  explicit VmtIntegritySensor(sim::World& world);
  VmtIntegrityReport collect();

 private:
  sim::World& world_;
};

// ── Focus correlation (app state monitoring) ──
struct FocusCorrelationReport {
  bool game_active = false;
  bool game_focused = false;
  int reads_during_inactive = 0;
  int total_reads = 0;
  double inactive_read_ratio = 0.0;
  bool anomalous = false;
  std::string detail;
};

/// Detects entity reads while game is alt-tabbed (a known cheat signal).
class FocusCorrelationSensor {
 public:
  explicit FocusCorrelationSensor(sim::World& world);
  FocusCorrelationReport analyze();

 private:
  sim::World& world_;
};

// ── Call-graph monitoring ──
struct CallGraphEdge {
  std::string from_function;
  std::string to_function;
  std::uint64_t tick;
  int call_count;
};

struct CallGraphReport {
  std::vector<CallGraphEdge> edges;
  int total_calls;
  int unique_callers;
  int unique_callees;
  bool anomalous_sequence;
  std::vector<std::string> detected_patterns;
  std::string detail;
};

/// Monitors function-call sequences and frequency for known cheat patterns.
class CallGraphSensor {
 public:
  explicit CallGraphSensor(sim::World& world);
  CallGraphReport analyze();

 private:
  sim::World& world_;
};

// ── Pattern frequency analysis ──
struct FunctionCallRecord {
  std::string function_name;
  std::uint64_t tick;
  std::uint64_t delta_from_last;
};

struct FrequencyReport {
  struct FunctionStats {
    std::string name;
    double avg_delta;
    double std_dev_delta;
    int call_count;
    bool consistent_interval;
    bool flagged;
  };
  std::vector<FunctionCallRecord> calls;
  std::vector<FunctionStats> stats;
  int flagged_functions;
  std::string detail;
};

/// Detects functions called at consistent intervals (cheat polling pattern).
class PatternFrequencySensor {
 public:
  explicit PatternFrequencySensor(sim::World& world);
  FrequencyReport analyze();

 private:
  sim::World& world_;
};

// ── Message frequency analyzer ──
struct MessageRecord {
  std::uint32_t message_id;
  std::uint64_t tick;
  bool server_requested;
};

struct MessageFrequencyReport {
  std::vector<MessageRecord> messages;
  int unsolicited_count;
  bool unsolicited_anomaly;
  int message_rate;
  bool rate_anomalous;
  bool expected_pattern_matched;
  std::string detail;
};

/// Detects anomalous network message patterns (spoofed clients).
class MessageFrequencySensor {
 public:
  explicit MessageFrequencySensor(sim::World& world);
  MessageFrequencyReport analyze();

 private:
  sim::World& world_;
};

// ── Event listener monitoring ──
struct EventListenerEntry {
  std::string event_name;
  std::string owning_module;
  bool is_game_module;
  bool is_cheat_module;
};

struct EventListenerReport {
  std::vector<EventListenerEntry> listeners;
  int total_listeners;
  int non_game_listeners;
  bool cheat_listener_detected;
  std::string detail;
};

/// Detects event listeners from unexpected modules.
class EventListenerSensor {
 public:
  explicit EventListenerSensor(sim::World& world);
  EventListenerReport analyze();

 private:
  sim::World& world_;
};

// ── Subtick analysis (counter-strafe) ──
struct TickDeltaRecord {
  int action;
  int tick_delta;
  int keys_pressed;
};

struct SubtickAnalysisReport {
  std::vector<TickDeltaRecord> deltas;
  int total_events;
  double mean_delta;
  double variance_delta;
  double std_dev_delta;
  int perfect_strafes;
  double perfect_ratio;
  bool macro_suspected;
  bool automation_suspected;
  double human_likelihood;
  std::string detail;
};

/// Analyzes counter-strafe subtick timing to detect macros and automation.
class SubtickAnalysisSensor {
 public:
  explicit SubtickAnalysisSensor(sim::World& world);
  SubtickAnalysisReport analyze();

 private:
  sim::World& world_;
};

// ── Three-level VMT protection ──
struct ModuleIntegrity {
  std::string module_name;
  std::uint64_t base_address = 0;
  std::uint32_t normalized_crc32 = 0;
  std::uint32_t expected_crc32 = 0;
  bool integrity_ok = true;
};

struct VmtIntegrityExtendedReport {
  // Level 1: Interface VMT CRC
  std::uint32_t crc_interfaces = 0;
  bool interface_tamper_detected = false;

  // Level 2: Entity VMT frequency
  int total_entity_vmts = 0;
  int unique_entity_vmts = 0;
  int single_occurrence_vmts = 0;
  bool anomalous_vmt_found = false;

  // Level 3: Module PE hash
  std::vector<ModuleIntegrity> module_integrity;
  bool tampered_module_found = false;

  std::string detail;
};

/// Three-level VMT integrity checker.
class VmtExtendedSensor {
 public:
  explicit VmtExtendedSensor(sim::World& world);
  VmtIntegrityExtendedReport check();

 private:
  sim::World& world_;
  std::uint32_t compute_crc32(const std::uint8_t* data, std::size_t size);
};

struct ModuleAnalysisReport {
  int modules_analyzed = 0;
  std::uint32_t module_crc32 = 0;
  bool suspicious_module_found = false;
  std::vector<DiagnosticModuleInfo> suspicious_modules;
  std::string detail;
};

// ── Full PE analysis ──
struct PeAnalysis {
  std::string module_name;
  std::uint64_t base = 0;
  std::uint32_t size = 0;
  std::uint16_t machine_type = 0;
  std::uint32_t pe_timestamp = 0;
  std::uint32_t checksum = 0;
  std::uint16_t section_count = 0;
  bool clr_present = false;
  std::uint32_t normalized_crc32 = 0;
  std::uint64_t sha1_hi = 0;
  std::uint64_t sha1_lo = 0;
  std::string pdb_path;
  bool integrity_ok = false;
};
class ModuleAnalysisSensor {
 public:
  explicit ModuleAnalysisSensor(sim::World& world);
  ModuleAnalysisReport analyze();
  PeAnalysis analyze_pe(std::uint32_t pid, const std::string& module_name);

 private:
  sim::World& world_;
};

struct ExceptionMonitorReport {
  int exception_count = 0;
  int access_violations = 0;
  int single_step_exceptions = 0;
  bool exception_handler_active = false;
  std::uint64_t veh_address = 0;
  bool code_cave_detected = false;
  bool self_patch_detected = false;
  std::string detail;
};
class ExceptionMonitorSensor {
 public:
  explicit ExceptionMonitorSensor(sim::World& world);
  ExceptionMonitorReport monitor();

 private:
  sim::World& world_;
};

struct CounterStrafeReport {
  int total_events = 0;
  double avg_tick_delta = 0.0;
  double std_dev_tick_delta = 0.0;
  int perfect_strafes = 0;
  double perfect_ratio = 0.0;
  bool macro_suspected = false;
  bool automation_suspected = false;
  std::string detail;
};
class CounterStrafeSensor {
 public:
  explicit CounterStrafeSensor(sim::World& world);
  CounterStrafeReport analyze();
  void record_event(int action, std::uint64_t tick, int keys_pressed);

 private:
  sim::World& world_;
};

struct CpuidVmReport {
  bool hypervisor_detected = false;
  std::uint32_t cpuid_packed = 0;
  std::string hv_vendor;
  std::string detail;
};
class CpuidVmSensor {
 public:
  explicit CpuidVmSensor(sim::World& world);
  CpuidVmReport detect();

 private:
  sim::World& world_;
};

struct DebugMonitorReport {
  int hardware_bps = 0;
  int software_bps = 0;
  bool debugger_attached = false;
  bool hardware_bp_detected = false;
  bool software_bp_detected = false;
  std::vector<cs2::DiagnosticTelemetryState::DebugBreakpoint> breakpoints;
  std::string detail;
};
class DebugMonitorSensor {
 public:
  explicit DebugMonitorSensor(sim::World& world);
  DebugMonitorReport scan();

 private:
  sim::World& world_;
};

struct MemoryDumpReport {
  std::uint64_t target_address = 0;
  std::size_t dump_size = 0;
  bool dump_performed = false;
  std::vector<std::uint8_t> data;
  std::string detail;
};
class MemoryDumpSensor {
 public:
  explicit MemoryDumpSensor(sim::World& world);
  MemoryDumpReport dump(std::uint64_t address, std::size_t size);

 private:
  sim::World& world_;
};

}  // namespace cs2::sensors
