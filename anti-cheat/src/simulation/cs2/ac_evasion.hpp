#pragma once

// Red-team evasion techniques against the simulated CS2 diagnostic system.
// All techniques operate only on sim::World fields.

#include "sim/world.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cs2::evasion {

struct EvasionTechniqueReport {
  std::string technique_name;
  bool applied = false;
  bool defeats_sensor = false;
  std::vector<std::string> sensors_defeated;
  std::string detail;
};

struct AntiDiagnosticReport {
  bool module_hiding_active = false;
  bool pe_timestamp_spoof_active = false;
  bool thread_obfuscation_active = false;
  bool convar_restore_active = false;
  bool vmt_protect_active = false;
  bool exception_suppress_active = false;
  bool counter_strafe_humanize_active = false;
  bool vm_detection_block_active = false;
  bool debug_monitor_clear_active = false;
  bool memory_dump_prevent_active = false;
  bool file_integrity_bypass_active = false;
  int technique_count = 0;
  double detection_probability = 1.0;
  std::vector<EvasionTechniqueReport> technique_reports;
  std::string detail;
};

bool hide_modules(sim::World& world, std::uint32_t cheat_pid);
bool spoof_pe_timestamps(sim::World& world);
bool obfuscate_thread(sim::World& world, std::uint32_t cheat_pid);
bool restore_convars(sim::World& world);
bool protect_vmts(sim::World& world);
bool suppress_exceptions(sim::World& world);
bool humanize_counter_strafe(sim::World& world);
bool block_vm_detection(sim::World& world);
bool clear_debug_monitor(sim::World& world);
bool prevent_memory_dump(sim::World& world, std::uint64_t sensitive_address,
                         std::size_t size);
bool bypass_file_integrity(sim::World& world);

AntiDiagnosticReport apply_all_evasions(sim::World& world,
                                        std::uint32_t cheat_pid);

}  // namespace cs2::evasion
