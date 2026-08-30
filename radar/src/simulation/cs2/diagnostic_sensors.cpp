#include "cs2/diagnostic_sensors.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>

#include "simulation/obfuscation.hpp"

namespace cs2::sensors {
namespace {

constexpr std::uint32_t kPageExecuteReadWrite = 0x40;
constexpr std::uint32_t kAccessViolation = 0xC0000005u;
constexpr std::uint32_t kSingleStep = 0x80000004u;

bool is_system_module(const std::string& name) {
  return name == OBF("kernel32.dll") || name == OBF("kernelbase.dll") || name == OBF("ntdll.dll");
}

bool is_game_module(const std::string& name) {
  return name == OBF("cs2.exe") || name == OBF("client.dll") || name == OBF("engine2.dll") ||
         name == OBF("gameoverlayrenderer64.dll");
}

bool is_expected_module(const std::string& name) {
  return is_system_module(name) || is_game_module(name) || name == OBF("steamclient64.dll");
}

std::uint32_t module_timestamp(const sim::World& world, const std::string& name) {
  if (name == OBF("client.dll")) return world.pe_timestamp_client_dll;
  if (name == OBF("kernel32.dll")) return world.pe_timestamp_kernel32;
  if (name == OBF("cs2.exe")) return world.pe_timestamp_cs2_exe;
  if (name == OBF("ntdll.dll")) return world.pe_timestamp_ntdll;
  if (name == OBF("gameoverlayrenderer64.dll")) return world.pe_timestamp_gameoverlay;
  return 0;
}

std::uint32_t expected_module_timestamp(const std::string& name) {
  if (name == OBF("client.dll")) return 0x66800000;
  if (name == OBF("kernel32.dll")) return 0x66000000;
  if (name == OBF("cs2.exe")) return 0x667F0000;
  if (name == OBF("ntdll.dll")) return 0x65FF0000;
  if (name == OBF("gameoverlayrenderer64.dll")) return 0x667E0000;
  return 0;
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
  std::uint32_t crc = 0xFFFFFFFFu;
  for (std::size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit != 8; ++bit)
      crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
  }
  return ~crc;
}

std::vector<std::uint8_t> normalized_module_bytes(const sim::Module& module) {
  std::vector<std::uint8_t> bytes(module.name.begin(), module.name.end());
  const auto* size = reinterpret_cast<const std::uint8_t*>(&module.size);
  bytes.insert(bytes.end(), size, size + sizeof(module.size));
  return bytes;
}

std::vector<std::uint8_t> observed_module_bytes(const sim::Module& module) {
  auto bytes = normalized_module_bytes(module);
  bytes.push_back(static_cast<std::uint8_t>(module.headers_erased));
  bytes.push_back(static_cast<std::uint8_t>(module.iat_hooked));
  bytes.push_back(static_cast<std::uint8_t>(module.eat_hooked));
  bytes.push_back(static_cast<std::uint8_t>(module.present_hooked));
  bytes.insert(bytes.end(), module.text_hash.begin(), module.text_hash.end());
  return bytes;
}

std::pair<std::uint64_t, std::uint64_t> sha1_prefix(
    const std::vector<std::uint8_t>& input) {
  std::vector<std::uint8_t> data = input;
  const std::uint64_t bit_length = static_cast<std::uint64_t>(data.size()) * 8;
  data.push_back(0x80);
  while ((data.size() % 64) != 56) data.push_back(0);
  for (int shift = 56; shift >= 0; shift -= 8) {
    data.push_back(static_cast<std::uint8_t>(bit_length >> shift));
  }

  std::uint32_t h0 = 0x67452301u;
  std::uint32_t h1 = 0xEFCDAB89u;
  std::uint32_t h2 = 0x98BADCFEu;
  std::uint32_t h3 = 0x10325476u;
  std::uint32_t h4 = 0xC3D2E1F0u;
  for (std::size_t offset = 0; offset < data.size(); offset += 64) {
    std::array<std::uint32_t, 80> words{};
    for (std::size_t i = 0; i < 16; ++i) {
      words[i] = (static_cast<std::uint32_t>(data[offset + i * 4]) << 24) |
                 (static_cast<std::uint32_t>(data[offset + i * 4 + 1]) << 16) |
                 (static_cast<std::uint32_t>(data[offset + i * 4 + 2]) << 8) |
                 static_cast<std::uint32_t>(data[offset + i * 4 + 3]);
    }
    for (std::size_t i = 16; i < words.size(); ++i) {
      const std::uint32_t value = words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16];
      words[i] = (value << 1) | (value >> 31);
    }
    std::uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
    for (std::size_t i = 0; i < words.size(); ++i) {
      const std::uint32_t f = i < 20 ? ((b & c) | (~b & d)) :
                              i < 40 ? (b ^ c ^ d) :
                              i < 60 ? ((b & c) | (b & d) | (c & d)) : (b ^ c ^ d);
      const std::uint32_t k = i < 20 ? 0x5A827999u : i < 40 ? 0x6ED9EBA1u :
                              i < 60 ? 0x8F1BBCDCu : 0xCA62C1D6u;
      const std::uint32_t rotated = (a << 5) | (a >> 27);
      const std::uint32_t temp = rotated + f + e + k + words[i];
      e = d;
      d = c;
      c = (b << 30) | (b >> 2);
      b = a;
      a = temp;
    }
    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;
  }
  return {
      (static_cast<std::uint64_t>(h0) << 32) | h1,
      (static_cast<std::uint64_t>(h2) << 32) | h3,
  };
}

}  // namespace

ModuleSnapshotSensor::ModuleSnapshotSensor(sim::World& world) : world_(world) {}

void ModuleSnapshotSensor::capture_modules_from_world() {
  auto& modules = world_.diagnostic_state.loaded_modules;
  modules.clear();
  for (const auto& [pid, process] : world_.processes) {
    for (const auto& module : process.modules) {
      DiagnosticModuleInfo info;
      info.process_id = pid;
      info.name = module.name;
      info.base = module.base;
      info.size = module.size;
      info.game_module = process.is_game || is_game_module(module.name);
      info.system_module = is_system_module(module.name);
      info.ac_module = process.is_ac;
      info.expected = info.ac_module || is_expected_module(module.name);
      info.pe_timestamp = module_timestamp(world_, module.name);
      info.suspicious = !info.expected || module.headers_erased || module.text_hash != "clean" ||
                        module.iat_hooked || module.eat_hooked || module.present_hooked;
      modules.push_back(std::move(info));
    }
  }
  world_.diag_module_snapshot_taken = true;
  world_.note("module snapshot captured=" + std::to_string(modules.size()));
}

ModuleSnapshotReport ModuleSnapshotSensor::scan() {
  capture_modules_from_world();
  ModuleSnapshotReport report;
  for (const auto& module : world_.diagnostic_state.loaded_modules) {
    ++report.total_modules;
    report.game_modules += module.game_module;
    report.system_modules += module.system_module;
    report.ac_modules += module.ac_module;
    if (!module.expected) report.unknown_details.push_back(module);
  }
  report.unknown_modules = static_cast<int>(report.unknown_details.size());
  report.has_suspicious_modules = std::any_of(
      world_.diagnostic_state.loaded_modules.begin(),
      world_.diagnostic_state.loaded_modules.end(),
      [](const DiagnosticModuleInfo& module) { return module.suspicious; });
  report.detail = "module snapshot modules=" + std::to_string(report.total_modules) +
                  " suspicious=" + std::to_string(report.unknown_modules);
  world_.note(report.detail);
  return report;
}

PETimestampSensor::PETimestampSensor(sim::World& world) : world_(world) {}

bool PETimestampSensor::check_timestamp(std::uint32_t actual, std::uint32_t expected,
                                         const char* name, PETimestampReport& report) {
  if (actual == expected) return true;
  report.any_mismatch = true;
  ++report.mismatched_count;
  report.mismatched_modules.emplace_back(name);
  return false;
}

PETimestampReport PETimestampSensor::verify() {
  PETimestampReport report;
  report.client_dll_valid = check_timestamp(world_.pe_timestamp_client_dll, 0x66800000, OBF("client.dll"), report);
  report.kernel32_valid = check_timestamp(world_.pe_timestamp_kernel32, 0x66000000, OBF("kernel32.dll"), report);
  report.cs2_exe_valid = check_timestamp(world_.pe_timestamp_cs2_exe, 0x667F0000, OBF("cs2.exe"), report);
  report.ntdll_valid = check_timestamp(world_.pe_timestamp_ntdll, 0x65FF0000, OBF("ntdll.dll"), report);
  report.gameoverlay_valid = check_timestamp(world_.pe_timestamp_gameoverlay, 0x667E0000, OBF("gameoverlayrenderer64.dll"), report);
  world_.diag_pe_timestamps_sent = true;
  report.detail = "PE timestamps mismatches=" + std::to_string(report.mismatched_count);
  world_.note(report.detail);
  return report;
}

ThreadCaptureSensor::ThreadCaptureSensor(sim::World& world) : world_(world) {}

ThreadCaptureReport ThreadCaptureSensor::capture() {
  ThreadCaptureReport report;
  auto& captured = world_.diagnostic_state.thread_capture;
  for (const auto& [pid, process] : world_.processes) {
    if (!process.is_game && !process.has_foreign_thread) continue;
    captured.thread_id = pid;
    captured.module_name = process.modules.empty() ? process.name : process.modules.front().name;
    captured.from_suspicious_module = process.has_foreign_thread || process.manual_mapped_region ||
                                       process.thread_hijacked || process.hollowed;
    captured.from_rwx_memory = process.manual_mapped_region;
    captured.memory_protection = captured.from_rwx_memory ? kPageExecuteReadWrite : 0x20;
    captured.start_address = process.base + (captured.from_rwx_memory ? process.memory.size() : 0x1000);
    captured.signature.assign(process.memory.begin(), process.memory.begin() +
        std::min<std::size_t>(4096, process.memory.size()));
    report.thread_captured = true;
    break;
  }
  report.thread_id = captured.thread_id;
  report.from_suspicious_module = captured.from_suspicious_module;
  report.from_rwx_memory = captured.from_rwx_memory;
  report.from_system_dll = is_system_module(captured.module_name);
  report.signature_captured = !captured.signature.empty();
  world_.diag_thread_captured = true;
  report.detail = "thread capture id=" + std::to_string(report.thread_id) +
                  " suspicious=" + std::to_string(report.from_suspicious_module);
  world_.note(report.detail);
  return report;
}

ConVarIntegritySensor::ConVarIntegritySensor(sim::World& world) : world_(world) {}

std::uint32_t ConVarIntegritySensor::hash_djb2(const std::string& str) const {
  std::uint32_t hash = 1171724434u;
  for (unsigned char ch : str) hash = ch * 33u + hash;
  return hash;
}

std::uint32_t ConVarIntegritySensor::compute_crc32(const std::uint8_t* data, std::size_t size) const {
  return crc32(data, size);
}

ConVarIntegrityReport ConVarIntegritySensor::check() {
  ConVarIntegrityReport report;
  std::vector<std::uint8_t> all;
  std::vector<std::uint8_t> replicated;
  for (auto& convar : world_.diagnostic_state.convars) {
    convar.name_hash = hash_djb2(convar.name);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&convar.name_hash);
    all.insert(all.end(), bytes, bytes + sizeof(convar.name_hash));
    ++report.total_convars;
    ++report.processed_mode1;
    if (convar.replicated) {
      replicated.insert(replicated.end(), bytes, bytes + sizeof(convar.name_hash));
      ++report.processed_mode2;
    }
    if (convar.tampered) {
      report.integrity_ok = false;
      ++report.tampered_count;
      report.tampered_convars.push_back(convar.name);
    }
  }
  report.crc_mode1 = compute_crc32(all.data(), all.size());
  report.crc_mode2 = compute_crc32(replicated.data(), replicated.size());
  world_.diagnostic_state.convar_crc_mode1 = report.crc_mode1;
  world_.diagnostic_state.convar_crc_mode2 = report.crc_mode2;
  if (world_.diagnostic_state.tampered_convar ||
      (world_.diagnostic_state.expected_convar_crc_mode1 != 0 &&
       report.crc_mode1 != world_.diagnostic_state.expected_convar_crc_mode1) ||
      (world_.diagnostic_state.expected_convar_crc_mode2 != 0 &&
       report.crc_mode2 != world_.diagnostic_state.expected_convar_crc_mode2)) {
    report.integrity_ok = false;
  }
  world_.diag_convar_checked = true;
  report.detail = "ConVar integrity total=" + std::to_string(report.total_convars) +
                  " tampered=" + std::to_string(report.tampered_count);
  world_.note(report.detail);
  return report;
}

VmtIntegritySensor::VmtIntegritySensor(sim::World& world) : world_(world) {}

VmtIntegrityReport VmtIntegritySensor::collect() {
  VmtIntegrityReport report;
  std::unordered_map<std::uint64_t, int> counts;
  std::vector<std::uint8_t> interfaces;
  for (const auto& entry : world_.diagnostic_state.vmts) ++counts[entry.address];
  for (const auto& entry : world_.diagnostic_state.vmts) {
    report.entity_vmt_count += entry.entity_vmt;
    report.interface_count += !entry.entity_vmt;
    if (!entry.entity_vmt) {
      const auto* bytes = reinterpret_cast<const std::uint8_t*>(&entry.address);
      interfaces.insert(interfaces.end(), bytes, bytes + sizeof(entry.address));
    }
    if (!entry.expected || counts[entry.address] == 1) report.suspicious_vmts.push_back(entry);
  }
  report.crc_interfaces = crc32(interfaces.data(), interfaces.size());
  report.anomalous_vmts = static_cast<int>(report.suspicious_vmts.size());
  report.vmt_hooking_detected = report.anomalous_vmts != 0;
  report.interface_tamper_detected = std::any_of(report.suspicious_vmts.begin(), report.suspicious_vmts.end(),
       [](const DiagnosticVmtEntry& entry) { return !entry.expected; });
  world_.diagnostic_state.vmt_crc32 = report.crc_interfaces;
  if (world_.diagnostic_state.expected_vmt_crc32 != 0 &&
      report.crc_interfaces != world_.diagnostic_state.expected_vmt_crc32) {
    report.interface_tamper_detected = true;
  }
  world_.diagnostic_state.anomalous_vmt = report.vmt_hooking_detected;
  world_.diag_vmt_collected = true;
  report.detail = "VMT collection interfaces=" + std::to_string(report.interface_count) +
                  " anomalous=" + std::to_string(report.anomalous_vmts);
  world_.note(report.detail);
  return report;
}

FocusCorrelationSensor::FocusCorrelationSensor(sim::World& world) : world_(world) {}

FocusCorrelationReport FocusCorrelationSensor::analyze() {
  FocusCorrelationReport report;
  const auto& state = world_.diagnostic_state;
  report.game_active = state.is_active || world_.diagnostic_system_active || world_.match_active;
  report.game_focused = report.game_active && state.has_focus;
  report.total_reads = static_cast<int>(world_.remote_read_ops);
  if (!report.game_focused) report.reads_during_inactive = report.total_reads;
  if (report.total_reads != 0) {
    report.inactive_read_ratio = static_cast<double>(report.reads_during_inactive) /
                                 static_cast<double>(report.total_reads);
  }
  report.anomalous = report.reads_during_inactive > 0;
  report.detail = "focus correlation reads=" + std::to_string(report.total_reads) +
                  " inactive=" + std::to_string(report.reads_during_inactive);
  world_.note(report.detail);
  return report;
}

VmtExtendedSensor::VmtExtendedSensor(sim::World& world) : world_(world) {}

std::uint32_t VmtExtendedSensor::compute_crc32(const std::uint8_t* data,
                                               std::size_t size) {
  return crc32(data, size);
}

VmtIntegrityExtendedReport VmtExtendedSensor::check() {
  VmtIntegrityExtendedReport report;
  std::unordered_map<std::uint64_t, int> entity_counts;
  std::vector<std::uint8_t> interfaces;
  for (const auto& entry : world_.diagnostic_state.vmts) {
    if (entry.entity_vmt) {
      ++report.total_entity_vmts;
      ++entity_counts[entry.address];
    } else {
      const auto* bytes = reinterpret_cast<const std::uint8_t*>(&entry.address);
      interfaces.insert(interfaces.end(), bytes, bytes + sizeof(entry.address));
      report.interface_tamper_detected = report.interface_tamper_detected || !entry.expected;
    }
  }
  report.crc_interfaces = compute_crc32(interfaces.data(), interfaces.size());
  if (world_.diagnostic_state.expected_vmt_crc32 != 0 &&
      report.crc_interfaces != world_.diagnostic_state.expected_vmt_crc32) {
    report.interface_tamper_detected = true;
  }
  report.unique_entity_vmts = static_cast<int>(entity_counts.size());
  for (const auto& [address, count] : entity_counts) {
    (void)address;
    report.single_occurrence_vmts += count == 1;
  }
  report.anomalous_vmt_found = report.single_occurrence_vmts != 0 ||
      std::any_of(world_.diagnostic_state.vmts.begin(), world_.diagnostic_state.vmts.end(),
                   [](const DiagnosticVmtEntry& entry) { return !entry.expected; });

  ModuleSnapshotSensor(world_).capture_modules_from_world();
  for (const auto& module_info : world_.diagnostic_state.loaded_modules) {
    ModuleIntegrity integrity;
    integrity.module_name = module_info.name;
    integrity.base_address = module_info.base;
    const sim::Process* process = world_.proc(module_info.process_id);
    if (process != nullptr) {
      const auto found = std::find_if(process->modules.begin(), process->modules.end(),
          [&module_info](const sim::Module& module) {
            return module.name == module_info.name && module.base == module_info.base;
          });
      if (found != process->modules.end()) {
        sim::Module expected = *found;
        expected.headers_erased = false;
        expected.text_hash = "clean";
        expected.iat_hooked = false;
        expected.eat_hooked = false;
        expected.present_hooked = false;
        const auto baseline = observed_module_bytes(expected);
        const auto observed = observed_module_bytes(*found);
        integrity.expected_crc32 = compute_crc32(baseline.data(), baseline.size());
        integrity.normalized_crc32 = compute_crc32(observed.data(), observed.size());
        integrity.integrity_ok = integrity.normalized_crc32 == integrity.expected_crc32;
      } else {
        integrity.integrity_ok = false;
      }
    } else {
      integrity.integrity_ok = false;
    }
    report.tampered_module_found = report.tampered_module_found || !integrity.integrity_ok;
    report.module_integrity.push_back(std::move(integrity));
  }
  report.detail = "extended VMT entities=" + std::to_string(report.total_entity_vmts) +
                  " singleton=" + std::to_string(report.single_occurrence_vmts) +
                  " module_tamper=" + std::to_string(report.tampered_module_found);
  world_.note(report.detail);
  return report;
}

ModuleAnalysisSensor::ModuleAnalysisSensor(sim::World& world) : world_(world) {}

ModuleAnalysisReport ModuleAnalysisSensor::analyze() {
  ModuleSnapshotSensor(world_).capture_modules_from_world();
  ModuleAnalysisReport report;
  std::vector<std::uint8_t> data;
  for (auto& module : world_.diagnostic_state.loaded_modules) {
    const auto* process = world_.proc(module.process_id);
    const auto found = process != nullptr && std::any_of(process->modules.begin(), process->modules.end(),
        [&module](const sim::Module& candidate) { return candidate.name == module.name && candidate.base == module.base; });
    module.crc32 = crc32(reinterpret_cast<const std::uint8_t*>(module.name.data()), module.name.size());
    module.sha1 = "simulated:" + module.name;
    module.section_count = found ? 5 : 0;
    module.pdb_path = module.name + ".pdb";
    module.machine_type = "AMD64";
    module.suspicious = module.suspicious || !found || module.section_count == 0;
    data.insert(data.end(), module.name.begin(), module.name.end());
    ++report.modules_analyzed;
    if (module.suspicious) report.suspicious_modules.push_back(module);
  }
  report.module_crc32 = crc32(data.data(), data.size());
  report.suspicious_module_found = !report.suspicious_modules.empty();
  report.detail = "module analysis modules=" + std::to_string(report.modules_analyzed) +
                  " suspicious=" + std::to_string(report.suspicious_modules.size());
  world_.note(report.detail);
  return report;
}

PeAnalysis ModuleAnalysisSensor::analyze_pe(std::uint32_t pid,
                                            const std::string& module_name) {
  PeAnalysis analysis;
  analysis.module_name = module_name;
  const sim::Process* process = world_.proc(pid);
  if (process == nullptr) return analysis;
  const auto found = std::find_if(process->modules.begin(), process->modules.end(),
      [&module_name](const sim::Module& module) { return module.name == module_name; });
  if (found == process->modules.end()) return analysis;

  analysis.base = found->base;
  analysis.size = static_cast<std::uint32_t>(std::min<std::size_t>(
      found->size, std::numeric_limits<std::uint32_t>::max()));
  analysis.machine_type = 0x8664;
  analysis.pe_timestamp = module_timestamp(world_, module_name);
  const auto normalized = normalized_module_bytes(*found);
  const auto observed = observed_module_bytes(*found);
  analysis.normalized_crc32 = crc32(normalized.data(), normalized.size());
  analysis.checksum = crc32(observed.data(), observed.size());
  analysis.section_count = found->headers_erased ? 0 : 5;
  analysis.clr_present = module_name.find("managed") != std::string::npos ||
                         module_name.find(".net") != std::string::npos ||
                         module_name.find("clr") != std::string::npos;
  const auto [sha1_hi, sha1_lo] = sha1_prefix(observed);
  analysis.sha1_hi = sha1_hi;
  analysis.sha1_lo = sha1_lo;
  analysis.pdb_path = module_name + ".pdb";
  analysis.integrity_ok = !found->headers_erased && found->text_hash == "clean" &&
                          !found->iat_hooked && !found->eat_hooked &&
                          !found->present_hooked &&
                          (expected_module_timestamp(module_name) == 0 ||
                           analysis.pe_timestamp == expected_module_timestamp(module_name));
  return analysis;
}

ExceptionMonitorSensor::ExceptionMonitorSensor(sim::World& world) : world_(world) {}

ExceptionMonitorReport ExceptionMonitorSensor::monitor() {
  ExceptionMonitorReport report;
  report.exception_count = world_.veh_exception_count;
  report.veh_address = world_.last_exception_address;
  for (const auto& exception : world_.diagnostic_state.exceptions) {
    report.access_violations += exception.access_violation || exception.code == kAccessViolation;
    report.single_step_exceptions += exception.single_step || exception.code == kSingleStep;
  }
  report.exception_handler_active = world_.veh_handlers != 0;
  report.code_cave_detected = std::any_of(world_.processes.begin(), world_.processes.end(),
      [](const auto& item) { return item.second.manual_mapped_region; });
  report.self_patch_detected = std::any_of(world_.processes.begin(), world_.processes.end(),
      [](const auto& item) { return std::any_of(item.second.modules.begin(), item.second.modules.end(),
          [](const sim::Module& module) { return module.text_hash != "clean"; }); });
  world_.diag_exception_handler_active = true;
  report.detail = "exception monitor count=" + std::to_string(report.exception_count);
  world_.note(report.detail);
  return report;
}

CounterStrafeSensor::CounterStrafeSensor(sim::World& world) : world_(world) {}

void CounterStrafeSensor::record_event(int action, std::uint64_t tick, int keys_pressed) {
  world_.diagnostic_state.counter_strafe_events.push_back({action, tick, keys_pressed});
  ++world_.cs_total_actions;
  world_.note("counter-strafe event tick=" + std::to_string(tick));
}

CounterStrafeReport CounterStrafeSensor::analyze() {
  CounterStrafeReport report;
  const auto& events = world_.diagnostic_state.counter_strafe_events;
  report.total_events = static_cast<int>(events.size());
  std::vector<double> deltas;
  for (std::size_t i = 1; i < events.size(); ++i) {
    const double delta = static_cast<double>(events[i].tick - events[i - 1].tick);
    deltas.push_back(delta);
    report.perfect_strafes += delta == 1.0;
  }
  if (!deltas.empty()) {
    for (double delta : deltas) report.avg_tick_delta += delta;
    report.avg_tick_delta /= deltas.size();
    for (double delta : deltas) report.std_dev_tick_delta += std::pow(delta - report.avg_tick_delta, 2);
    report.std_dev_tick_delta = std::sqrt(report.std_dev_tick_delta / deltas.size());
    report.perfect_ratio = static_cast<double>(report.perfect_strafes) / deltas.size();
  }
  report.macro_suspected = deltas.size() >= 5 && report.perfect_ratio >= 0.8;
  report.automation_suspected = deltas.size() >= 5 && report.std_dev_tick_delta < 0.1;
  world_.cs_perfect_frames = report.perfect_strafes;
  world_.cs_average_tick_delta = report.avg_tick_delta;
  world_.diag_counter_strafe_monitored = true;
  report.detail = "counter-strafe events=" + std::to_string(report.total_events) +
                  " perfect=" + std::to_string(report.perfect_strafes);
  world_.note(report.detail);
  return report;
}

CpuidVmSensor::CpuidVmSensor(sim::World& world) : world_(world) {}

CpuidVmReport CpuidVmSensor::detect() {
  CpuidVmReport report;
  report.hypervisor_detected = world_.trust.personal_hv_active || world_.trust.platform_hv_active;
  report.hv_vendor = world_.trust.personal_hv_active ? world_.trust.hv_vendor :
                     (world_.trust.platform_hv_active ? world_.trust.platform_hv_vendor : "");
  report.cpuid_packed = report.hypervisor_detected ? 0x80000000u : 0u;
  world_.diagnostic_state.cpuid_hypervisor_present = report.hypervisor_detected;
  world_.diagnostic_state.cpuid_hypervisor_vendor = report.hv_vendor;
  world_.diag_cpuid_vm_checked = true;
  report.detail = "CPUID hypervisor=" + std::to_string(report.hypervisor_detected);
  world_.note(report.detail);
  return report;
}

DebugMonitorSensor::DebugMonitorSensor(sim::World& world) : world_(world) {}

DebugMonitorReport DebugMonitorSensor::scan() {
  DebugMonitorReport report;
  report.debugger_attached = world_.diagnostic_state.debugger_attached ||
                             world_.diagnostic_state.dll_verification.debugger_attached;
  auto collect = [&report](const auto& breakpoints, bool hardware) {
    for (const auto& breakpoint : breakpoints) {
      if (!breakpoint.enabled) continue;
      report.breakpoints.push_back(breakpoint);
      if (hardware) ++report.hardware_bps; else ++report.software_bps;
    }
  };
  collect(world_.diagnostic_state.dll_verification.breakpoints, false);
  collect(world_.diagnostic_state.hardware_breakpoints, true);
  collect(world_.diagnostic_state.software_breakpoints, false);
  report.hardware_bp_detected = report.hardware_bps != 0;
  report.software_bp_detected = report.software_bps != 0;
  world_.diag_debugger_checked = true;
  report.detail = "debug monitor hardware=" + std::to_string(report.hardware_bps) +
                  " software=" + std::to_string(report.software_bps);
  world_.note(report.detail);
  return report;
}

MemoryDumpSensor::MemoryDumpSensor(sim::World& world) : world_(world) {}

MemoryDumpReport MemoryDumpSensor::dump(std::uint64_t address, std::size_t size) {
  MemoryDumpReport report;
  report.target_address = address;
  if (const auto* game = world_.proc(world_.game_pid())) {
    const std::uint64_t relative = address >= game->base ? address - game->base : address;
    if (relative < game->memory.size()) {
      const auto offset = static_cast<std::size_t>(relative);
      report.dump_size = std::min(size, game->memory.size() - offset);
      report.data.assign(game->memory.begin() + offset, game->memory.begin() + offset + report.dump_size);
      report.dump_performed = true;
    }
  }
  world_.diag_memory_dump_performed = true;
  report.detail = "memory dump bytes=" + std::to_string(report.dump_size) +
                  " performed=" + std::to_string(report.dump_performed);
  world_.note(report.detail);
  return report;
}

}  // namespace cs2::sensors
