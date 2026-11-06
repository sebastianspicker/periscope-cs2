#pragma once

// Simulated state captured by CS2 diagnostic sensors. These types model
// telemetry payloads only; they never represent or invoke host OS APIs.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cs2 {

struct DiagnosticModuleInfo {
  std::uint32_t process_id = 0;
  std::string name;
  std::uint64_t base = 0;
  std::size_t size = 0;
  bool game_module = false;
  bool system_module = false;
  bool ac_module = false;
  bool expected = false;
  bool suspicious = false;
  std::uint32_t pe_timestamp = 0;
  std::uint32_t crc32 = 0;
  std::string sha1;
  int section_count = 0;
  std::string pdb_path;
  std::string machine_type;
};

struct ThreadCaptureState {
  std::uint32_t thread_id = 0;
  std::uint64_t start_address = 0;
  std::string module_name;
  std::uint32_t memory_protection = 0;
  std::vector<std::uint8_t> signature;
  bool from_suspicious_module = false;
  bool from_rwx_memory = false;
};

struct SimulatedConVar {
  std::string name;
  std::string value;
  bool replicated = false;
  bool tampered = false;
  std::uint32_t name_hash = 0;
};

struct DiagnosticVmtEntry {
  std::uint64_t address = 0;
  std::string owner;
  std::string module_name;
  bool entity_vmt = false;
  bool expected = true;
};

struct ExceptionRecord {
  // Stack capture simulation (CS2 VEH 2944-byte dump).
  struct StackFrame {
    std::uint64_t return_address = 0;
    std::string module_name;
    bool in_cheat_module = false;
  };

  std::uint32_t code = 0;
  std::uint64_t address = 0;
  bool access_violation = false;
  bool single_step = false;
  std::vector<StackFrame> stack_frames;
  bool contains_cheat_frames = false;
  std::uint32_t stack_depth = 0;
};

struct CounterStrafeEvent {
  int action = 0;
  std::uint64_t tick = 0;
  int keys_pressed = 0;
};

// Pattern-analysis telemetry for the educational diagnostic simulation.
struct DiagnosticCallRecord {
  std::string from_function;
  std::string to_function;
  std::uint64_t tick = 0;
};

struct DiagnosticMessageRecord {
  std::uint32_t message_id = 0;
  std::uint64_t tick = 0;
  bool server_requested = false;
};

struct DiagnosticEventListener {
  std::string event_name;
  std::string owning_module;
  bool is_game_module = false;
  bool is_cheat_module = false;
};

struct DiagnosticTelemetryState {
  struct DebugBreakpoint {
    std::uint64_t address = 0;
    bool hardware = false;
    bool enabled = true;
  };

  struct DebugMonitorState {
    bool debugger_attached = false;
    std::vector<DebugBreakpoint> breakpoints;
  };

  // Server-directed instrumentation (Type 30), represented only as
  // telemetry state inside the educational simulation.
  struct ServerInstrumentation {
    struct LinkedListNode {
      std::uint64_t data[5]{};
      std::uint64_t decoded_pointer = 0;
    };

    std::uint64_t tracking_ptr1 = 0;
    std::uint64_t tracking_ptr2 = 0;
    std::vector<LinkedListNode> nodes;
    bool active = false;
  };

  bool debugger_attached = false;
  std::vector<DebugBreakpoint> breakpoints;
  DebugMonitorState dll_verification;
  std::vector<DiagnosticModuleInfo> loaded_modules;
  ThreadCaptureState thread_capture;
  std::vector<SimulatedConVar> convars;
  std::vector<DiagnosticVmtEntry> vmts;
  std::vector<ExceptionRecord> exceptions;
  std::vector<CounterStrafeEvent> counter_strafe_events;
  std::vector<DiagnosticCallRecord> call_records;
  std::vector<DiagnosticMessageRecord> message_records;
  std::vector<DiagnosticEventListener> event_listeners;

  // Message 159 system-state fields.
  std::uint32_t pid = 0;
  std::uint32_t os_type = 0;
  std::uint64_t system_time = 0;
  bool is_active = false;
  bool has_focus = false;
  int total_files = 0;
  int files_need_check = 0;
  int files_completed = 0;

  // Simulated expected/baseline values used by the diagnostic exercises.
  std::uint32_t pe_timestamp_client_dll = 0x66800000;
  std::uint32_t expected_pe_timestamp_client_dll = 0x66800000;
  std::uint32_t pe_timestamp_engine2_dll = 0;
  std::uint32_t expected_pe_timestamp_engine2_dll = 0;
  std::uint32_t pe_timestamp_tier0_dll = 0;
  std::uint32_t expected_pe_timestamp_tier0_dll = 0;
  std::uint32_t convar_crc_mode1 = 0;
  std::uint32_t expected_convar_crc_mode1 = 0;
  std::uint32_t convar_crc_mode2 = 0;
  std::uint32_t expected_convar_crc_mode2 = 0;
  std::uint32_t vmt_crc32 = 0;
  std::uint32_t expected_vmt_crc32 = 0;
  bool tampered_convar = false;
  bool anomalous_vmt = false;
  bool thread_start_obfuscated = false;
  bool cpuid_hypervisor_present = false;
  std::string cpuid_hypervisor_vendor;
  std::vector<DebugBreakpoint> hardware_breakpoints;
  std::vector<DebugBreakpoint> software_breakpoints;
  bool b_secure_allowed = true;
  bool suppress_flag = false;
  ServerInstrumentation instrumentation;
};

}  // namespace cs2
