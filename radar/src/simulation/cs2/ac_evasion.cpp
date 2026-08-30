#include "cs2/ac_evasion.hpp"

#include <algorithm>
#include <utility>

namespace cs2::evasion {
namespace {

constexpr double kHumanCounterStrafeTickDelta = 1.35;
constexpr int kHumanCounterStrafeJitter = 3;
constexpr std::uint32_t kClientDllTimestamp = 0x66800000;
constexpr std::uint32_t kCs2ExeTimestamp = 0x667F0000;
constexpr std::uint32_t kKernel32Timestamp = 0x66000000;
constexpr std::uint32_t kNtdllTimestamp = 0x65FF0000;
constexpr std::uint32_t kGameoverlayTimestamp = 0x667E0000;

EvasionTechniqueReport make_report(std::string name, bool applied,
                                   std::vector<std::string> sensors,
                                   std::string detail) {
  return {
      .technique_name = std::move(name),
      .applied = applied,
      .defeats_sensor = applied,
      .sensors_defeated = std::move(sensors),
      .detail = std::move(detail),
  };
}

}  // namespace

bool hide_modules(sim::World& world, std::uint32_t cheat_pid) {
  sim::Process* cheat = world.proc(cheat_pid);
  if (cheat == nullptr) {
    world.note("cs2 evasion: module hiding failed; cheat process was not found");
    return false;
  }

  cheat->hidden_from_weak_enum = true;
  for (sim::Module& module : cheat->modules) module.linked_in_peb = false;
  world.note("cs2 evasion: cheat modules hidden from simulated enumeration");
  return true;
}

bool spoof_pe_timestamps(sim::World& world) {
  world.pe_timestamp_client_dll = kClientDllTimestamp;
  world.pe_timestamp_cs2_exe = kCs2ExeTimestamp;
  world.pe_timestamp_kernel32 = kKernel32Timestamp;
  world.pe_timestamp_ntdll = kNtdllTimestamp;
  world.pe_timestamp_gameoverlay = kGameoverlayTimestamp;
  world.note("cs2 evasion: simulated PE timestamps restored to expected values");
  return true;
}

bool obfuscate_thread(sim::World& world, std::uint32_t cheat_pid) {
  if (world.proc(cheat_pid) == nullptr) {
    world.note("cs2 evasion: thread obfuscation failed; cheat process was not found");
    return false;
  }

  world.thread_hide_from_debugger = true;
  world.peb_being_debugged_spoofed = true;
  world.note("cs2 evasion: simulated cheat thread start obfuscated");
  return true;
}

bool restore_convars(sim::World& world) {
  for (cs2::SimulatedConVar& convar : world.diagnostic_state.convars) {
    convar.tampered = false;
  }
  world.note("cs2 evasion: simulated ConVar integrity values restored");
  return true;
}

bool protect_vmts(sim::World& world) {
  for (cs2::DiagnosticVmtEntry& vmt : world.diagnostic_state.vmts) vmt.expected = true;
  world.note("cs2 evasion: simulated VMT integrity values restored");
  return true;
}

bool suppress_exceptions(sim::World& world) {
  world.diagnostic_state.exceptions.clear();
  world.veh_exception_count = 0;
  world.note("cs2 evasion: simulated exception telemetry cleared");
  return true;
}

bool humanize_counter_strafe(sim::World& world) {
  world.cs_average_tick_delta = kHumanCounterStrafeTickDelta;
  world.cs_perfect_frames =
      std::max(0, world.cs_perfect_frames - kHumanCounterStrafeJitter);
  world.note("cs2 evasion: simulated counter-strafe timing humanized");
  return true;
}

bool block_vm_detection(sim::World& world) {
  world.trust.personal_hv_active = false;
  world.trust.hv_vendor.clear();
  world.note("cs2 evasion: simulated CPUID hypervisor indicators cleared");
  return true;
}

bool clear_debug_monitor(sim::World& world) {
  world.diagnostic_state.dll_verification.breakpoints.clear();
  world.diagnostic_state.dll_verification.debugger_attached = false;
  world.note("cs2 evasion: simulated debug monitor breakpoint state cleared");
  return true;
}

bool prevent_memory_dump(sim::World& world, std::uint64_t sensitive_address,
                         std::size_t size) {
  sim::Process* game = world.proc(world.game_pid());
  if (game == nullptr || sensitive_address < game->base) {
    world.note("cs2 evasion: simulated memory dump prevention failed; invalid range");
    return false;
  }

  const std::uint64_t offset = sensitive_address - game->base;
  if (offset >= game->memory.size()) {
    world.note("cs2 evasion: simulated memory dump prevention failed; range outside game memory");
    return false;
  }

  const std::size_t begin = static_cast<std::size_t>(offset);
  const std::size_t count = std::min(size, game->memory.size() - begin);
  std::fill_n(game->memory.begin() + static_cast<std::ptrdiff_t>(begin), count, 0);
  world.note("cs2 evasion: simulated sensitive memory range zeroed before dump");
  return true;
}

bool bypass_file_integrity(sim::World& world) {
  world.client_allowed_on_secure_servers = true;
  world.diag_file_integrity_checked = true;
  world.note("cs2 evasion: simulated secure-file integrity result bypassed");
  return true;
}

AntiDiagnosticReport apply_all_evasions(sim::World& world,
                                        std::uint32_t cheat_pid) {
  AntiDiagnosticReport report;
  const std::uint64_t dump_address = [&world] {
    const sim::Process* game = world.proc(world.game_pid());
    return game == nullptr ? 0ull : game->base;
  }();

  const auto add = [&report](EvasionTechniqueReport technique, bool& active) {
    active = technique.applied;
    report.technique_count += technique.applied ? 1 : 0;
    report.technique_reports.push_back(std::move(technique));
  };

  add(make_report("module_hiding", hide_modules(world, cheat_pid),
                  {"ModuleSnapshotSensor"}, "Cheat modules unlinked in simulation."),
      report.module_hiding_active);
  add(make_report("pe_timestamp_spoofing", spoof_pe_timestamps(world),
                  {"PETimestampSensor"}, "Expected simulated PE timestamps restored."),
      report.pe_timestamp_spoof_active);
  add(make_report("thread_obfuscation", obfuscate_thread(world, cheat_pid),
                  {"ThreadCaptureSensor"}, "Simulated thread start obfuscated."),
      report.thread_obfuscation_active);
  add(make_report("convar_restore", restore_convars(world),
                  {"ConVarIntegritySensor"}, "Expected simulated ConVar CRCs restored."),
      report.convar_restore_active);
  add(make_report("vmt_protection", protect_vmts(world),
                  {"VmtIntegritySensor"}, "Expected simulated VMT CRC restored."),
      report.vmt_protect_active);
  add(make_report("exception_suppression", suppress_exceptions(world),
                  {"ExceptionMonitorSensor"}, "Simulated exception history cleared."),
      report.exception_suppress_active);
  add(make_report("counter_strafe_humanization", humanize_counter_strafe(world),
                  {"CounterStrafeSensor"}, "Simulated timing variance applied."),
      report.counter_strafe_humanize_active);
  add(make_report("vm_detection_blocking", block_vm_detection(world),
                  {"CpuidVmSensor"}, "Simulated CPUID VM indicators cleared."),
      report.vm_detection_block_active);
  add(make_report("debug_monitor_clearing", clear_debug_monitor(world),
                  {"DebugMonitorSensor"}, "Simulated breakpoint lists cleared."),
      report.debug_monitor_clear_active);
  add(make_report("memory_dump_prevention",
                  dump_address != 0 && prevent_memory_dump(world, dump_address, 64),
                  {"MemoryDumpSensor"}, "Simulated dump range zeroed."),
      report.memory_dump_prevent_active);
  add(make_report("file_integrity_bypass", bypass_file_integrity(world),
                  {"ModuleAnalysisSensor"}, "Simulated secure-file flag set."),
      report.file_integrity_bypass_active);

  report.detection_probability =
      std::max(0.0, 1.0 - static_cast<double>(report.technique_count) / 11.0);
  report.detail = "Applied " + std::to_string(report.technique_count) +
                  " of 11 simulated anti-diagnostic evasions.";
  world.note("cs2 evasion: maximum simulated anti-diagnostic mode applied");
  return report;
}

}  // namespace cs2::evasion
