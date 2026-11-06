// diagnostic_system.hpp — Realistic model of CS2's CDllVerificationMonitor.
//
// Based on public reverse-engineering of CS2's built-in anti-tamper:
//   - Message 159 (DllStatusResponse): 40+ diagnostic fields
//   - Message 157 (UtilAction): ConVar CRC integrity
//   - Message 160 (Inventory): VMT pointers + module PE analysis
//
// Educational: This models what the REAL CS2 anti-cheat collects.
// Strategy pairs exercise these exact detection vectors.

#pragma once

#include "ac/types.hpp"
#include "sim/world.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cs2 {

/// Simulates a loaded module entry as tracked by CS2's diagnostic system.
struct ModuleSnapshotEntry {
  std::string name;
  uint64_t base = 0;
  size_t size = 0;
};

/// Simulates a suspicious thread detected by MonitorThreadContext.
struct ThreadContextEntry {
  void* start_address = nullptr;
  void* module_handle = nullptr;
  uint32_t thread_id = 0;
  uint32_t memory_protection = 0;
  void* return_address = nullptr;
  void* allocation_base = nullptr;
  std::array<uint8_t, 4096> signature_buffer{};
  uint32_t signature_size = 0;

  bool is_suspicious() const {
    // Real conditions from CS2's MonitorThreadContext:
    // 1. PAGE_EXECUTE_READWRITE protection (RWX)
    // 2. Start address in kernel32.dll or kernelbase.dll
    // 3. No backing module
    return (memory_protection == 0x40) || (module_handle == nullptr);
  }
};

/// PE timestamp data (TimeDateStamp from IMAGE_FILE_HEADER).
struct PeTimestampData {
  uint32_t client_dll = 0;
  uint32_t cs2_exe = 0;
  uint32_t kernel32 = 0;
  uint32_t ntdll = 0;
  uint32_t gameoverlay = 0;
};

/// ConVar integrity result (CRC32 matching).
struct ConVarIntegrityResult {
  uint32_t crc_all = 0;
  uint32_t crc_replicated = 0;
  int count_all = 0;
  int count_replicated = 0;
  bool match = true;
};

/// VMT (Virtual Method Table) entry.
struct VmtEntry {
  uint64_t address = 0;
  int usage_count = 0;
  uint64_t base_offset = 0;
  bool anomalous = false;
};

/// Module PE analysis result (from CollectAndSendModuleInventory).
struct PeModuleAnalysis {
  std::string name;
  uint32_t name_hash = 0;
  size_t image_size = 0;
  uint32_t timestamp = 0;
  uint32_t checksum = 0;
  uint16_t machine_type = 0;
  uint8_t sha1_hash[20]{};
  uint32_t section_count = 0;
};

/// Full CDllVerificationMonitor state model.
/// Mirrors the real CDllVerificationMonitor structure from CS2's client.dll.
struct DllVerificationState {
  std::vector<ModuleSnapshotEntry> loaded_modules;
  int total_files_loaded = 0;
  int files_need_trust_check = 0;
  int files_completed_trust_check = 0;

  std::vector<ThreadContextEntry> suspicious_threads;
  int thread_total_count = 0;

  PeTimestampData pe_timestamps;

  bool b_secure_allowed = true;
  std::string violation_report;
  bool suppress_flag = false;
  bool client_allowed_on_secure = true;

  uint32_t process_id = 0;
  uint32_t active_thread_id = 0;
  bool has_focus = false;
  bool is_active = false;
  bool debugger_detected = false;
  uint64_t cpuid_vm_result = 0;

  ConVarIntegrityResult convar_integrity;

  std::vector<VmtEntry> interface_vmts;
  std::vector<VmtEntry> entity_vmts;
  uint32_t vmt_crc = 0;

  std::vector<PeModuleAnalysis> analyzed_modules;
  uint32_t module_crc = 0;
  int module_count = 0;

  /// Builds a Message 159-style snapshot from the simulated game process.
  static DllVerificationState from_world(const sim::World& world);

  /// Reports whether the collected anti-tamper telemetry contains a violation.
  bool has_violations() const;

  /// Produces a human-readable educational diagnostic report.
  std::string describe() const;

  /// Produces a stable text representation of Message 159 telemetry.
  std::string serialize_message_159() const;

  /// Compares the simulated Message 157 ConVar CRCs with the server baseline.
  bool check_convar_integrity(const ConVarIntegrityResult& expected);

  /// Collects Message 160-style VMT and PE inventory summaries.
  void collect_inventory();
};

/// Detects external process handles that remain visible to system-wide enumeration.
struct HandleTableEntry {
  uint32_t pid;
  uint64_t handle;
  uint32_t access_mask;
  std::string process_name;
};

std::vector<HandleTableEntry> scan_handle_table_for_game(
    const sim::World& world, uint32_t game_pid);

/// Checks a thread start tuple using the simulated MonitorThreadContext rules.
bool is_thread_start_suspicious(void* start_address, uint32_t protection,
                                void* module_handle);

}  // namespace cs2
