// diagnostic_system.cpp — Educational CDllVerificationMonitor model.

#include "cs2/diagnostic_system.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <sstream>
#include <unordered_map>

#include "simulation/obfuscation.hpp"

namespace cs2 {
namespace {

constexpr uint32_t kPageExecuteReadWrite = 0x40;
constexpr uint32_t kPageExecuteRead = 0x20;
constexpr uint16_t kAmd64Machine = 0x8664;
constexpr uintptr_t kKernel32ModuleHandle = 0x76000000u;
constexpr uintptr_t kKernelBaseModuleHandle = 0x77000000u;

// Real CS2 hashes collected byte ranges. This deterministic CRC32 substitutes for
// those ranges without reading host memory in the educational simulation.
uint32_t crc32(const uint8_t* bytes, size_t size) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < size; ++i) {
    crc ^= bytes[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ ((crc & 1u) == 0 ? 0u : 0xEDB88320u);
    }
  }
  return ~crc;
}

uint32_t hash_name(const std::string& value) {
  // Mirrors the stable name hash used to index a collected module/ConVar record.
  // Name changes are a red artifact; blue compares the result with its baseline.
  uint32_t hash = 2166136261u;
  for (unsigned char ch : value) hash = (hash ^ ch) * 16777619u;
  return hash;
}

bool is_expected_module(const std::string& name) {
  // Mirrors CS2's allowlisted runtime-module classification. Unrecognized names
  // are the injected-DLL scar that blue marks for a trust verification failure.
  return name == OBF("game.exe") || name == OBF("cs2.exe") || name == OBF("client.dll") ||
         name == OBF("engine2.dll") || name == OBF("tier0.dll") ||
         name == OBF("kernel32.dll") || name == OBF("kernelbase.dll") ||
         name == OBF("ntdll.dll") || name == OBF("gameoverlayrenderer64.dll") ||
         name == OBF("steamclient64.dll");
}

uint32_t timestamp_for(const sim::World& world, const std::string& name) {
  // Mirrors IMAGE_FILE_HEADER.TimeDateStamp collection for known runtime images.
  // A replaced image leaves a mismatched timestamp for blue's server comparison.
  if (name == OBF("client.dll")) return world.pe_timestamp_client_dll;
  if (name == OBF("game.exe") || name == OBF("cs2.exe")) return world.pe_timestamp_cs2_exe;
  if (name == OBF("kernel32.dll")) return world.pe_timestamp_kernel32;
  if (name == OBF("ntdll.dll")) return world.pe_timestamp_ntdll;
  if (name == OBF("gameoverlayrenderer64.dll")) return world.pe_timestamp_gameoverlay;
  return 0;
}

void append_u64(std::vector<uint8_t>& output, uint64_t value) {
  // Mirrors appending a pointer-sized inventory value to an integrity buffer.
  // VMT redirection changes the bytes, allowing blue to detect a changed CRC.
  const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
  output.insert(output.end(), bytes, bytes + sizeof(value));
}

}  // namespace

// Mirrors CDllVerificationMonitor's assembly of Message 159 state. Red's module,
// manual-map, hook, debugger, and hypervisor scars become server-visible fields;
// blue detects them by correlating the resulting trust and integrity failures.
DllVerificationState DllVerificationState::from_world(const sim::World& world) {
  DllVerificationState state;
  const sim::Process* game = world.proc(world.game_pid());
  state.process_id = game == nullptr ? 0 : game->pid;
  state.has_focus = world.match_active;
  state.is_active = world.diagnostic_system_active || world.match_active;
  state.debugger_detected = world.analysis_host || world.diagnostic_state.debugger_attached ||
                            world.thread_hide_from_debugger;
  state.cpuid_vm_result = (world.trust.personal_hv_active || world.trust.platform_hv_active)
                              ? 1u
                              : 0u;
  state.pe_timestamps = {world.pe_timestamp_client_dll, world.pe_timestamp_cs2_exe,
                         world.pe_timestamp_kernel32, world.pe_timestamp_ntdll,
                         world.pe_timestamp_gameoverlay};

  if (game != nullptr) {
    for (const sim::Module& module : game->modules) {
      state.loaded_modules.push_back({module.name, module.base, module.size});
      const bool trust_issue = !is_expected_module(module.name) || !module.linked_in_peb ||
                               module.headers_erased || module.text_hash != "clean" ||
                               module.iat_hooked || module.eat_hooked || module.present_hooked;
      ++state.total_files_loaded;
      state.files_need_trust_check += trust_issue;
      state.files_completed_trust_check += !trust_issue;
    }

    // The world does not expose host threads. Its foreign/manual-map/hijack flags
    // model the same thread-table scar that MonitorThreadContext records.
    ++state.thread_total_count;
    if (game->has_foreign_thread || game->manual_mapped_region || game->thread_hijacked) {
      ThreadContextEntry thread;
      thread.thread_id = game->pid;
      thread.start_address = reinterpret_cast<void*>(static_cast<uintptr_t>(
          game->base + (game->manual_mapped_region ? game->memory.size() : 0x1000)));
      thread.module_handle = game->manual_mapped_region ? nullptr :
          reinterpret_cast<void*>(static_cast<uintptr_t>(game->base));
      thread.memory_protection = game->manual_mapped_region ? kPageExecuteReadWrite : kPageExecuteRead;
      thread.return_address = thread.start_address;
      thread.allocation_base = thread.module_handle;
      thread.signature_size = static_cast<uint32_t>(std::min<size_t>(
          thread.signature_buffer.size(), game->memory.size()));
      std::copy_n(game->memory.begin(), thread.signature_size, thread.signature_buffer.begin());
      if (is_thread_start_suspicious(thread.start_address, thread.memory_protection,
                                     thread.module_handle)) {
        state.suspicious_threads.push_back(thread);
      }
      state.active_thread_id = thread.thread_id;
    }

    std::unordered_map<uint64_t, int> vmt_usage;
    for (const DiagnosticVmtEntry& entry : world.diagnostic_state.vmts) {
      ++vmt_usage[entry.address];
    }
    for (const DiagnosticVmtEntry& entry : world.diagnostic_state.vmts) {
      VmtEntry snapshot;
      snapshot.address = entry.address;
      snapshot.usage_count = vmt_usage[entry.address];
      snapshot.base_offset = entry.address >= game->base ? entry.address - game->base : 0;
      snapshot.anomalous = !entry.expected || snapshot.usage_count == 1;
      (entry.entity_vmt ? state.entity_vmts : state.interface_vmts).push_back(snapshot);
    }

    for (const sim::Module& module : game->modules) {
      PeModuleAnalysis analysis;
      analysis.name = module.name;
      analysis.name_hash = hash_name(module.name);
      analysis.image_size = module.size;
      analysis.timestamp = timestamp_for(world, module.name);
      std::vector<uint8_t> observed(module.name.begin(), module.name.end());
      append_u64(observed, module.base);
      append_u64(observed, module.size);
      observed.push_back(static_cast<uint8_t>(module.linked_in_peb));
      observed.push_back(static_cast<uint8_t>(module.headers_erased));
      observed.push_back(static_cast<uint8_t>(module.iat_hooked));
      observed.push_back(static_cast<uint8_t>(module.eat_hooked));
      observed.push_back(static_cast<uint8_t>(module.present_hooked));
      observed.insert(observed.end(), module.text_hash.begin(), module.text_hash.end());
      analysis.checksum = crc32(observed.data(), observed.size());
      analysis.machine_type = kAmd64Machine;
      analysis.section_count = module.headers_erased ? 0 : 5;
      for (size_t index = 0; index < sizeof(analysis.sha1_hash); ++index) {
        analysis.sha1_hash[index] = static_cast<uint8_t>(
            (analysis.checksum >> ((index % sizeof(analysis.checksum)) * 8)) ^ index);
      }
      state.analyzed_modules.push_back(analysis);
    }
  }

  std::vector<uint8_t> all_convars;
  std::vector<uint8_t> replicated_convars;
  for (const SimulatedConVar& convar : world.diagnostic_state.convars) {
    const uint32_t name_hash = hash_name(convar.name);
    const auto* bytes = reinterpret_cast<const uint8_t*>(&name_hash);
    all_convars.insert(all_convars.end(), bytes, bytes + sizeof(name_hash));
    ++state.convar_integrity.count_all;
    if (convar.replicated) {
      replicated_convars.insert(replicated_convars.end(), bytes, bytes + sizeof(name_hash));
      ++state.convar_integrity.count_replicated;
    }
    state.convar_integrity.match &= !convar.tampered;
  }
  state.convar_integrity.crc_all = crc32(all_convars.data(), all_convars.size());
  state.convar_integrity.crc_replicated = crc32(replicated_convars.data(), replicated_convars.size());
  state.convar_integrity.match &= !world.diagnostic_state.tampered_convar;
  state.collect_inventory();

  const bool module_violation = state.files_need_trust_check != 0;
  const bool vmt_violation = std::any_of(state.interface_vmts.begin(), state.interface_vmts.end(),
      [](const VmtEntry& entry) { return entry.anomalous; }) ||
      std::any_of(state.entity_vmts.begin(), state.entity_vmts.end(),
      [](const VmtEntry& entry) { return entry.anomalous; });
  state.b_secure_allowed = !module_violation && state.suspicious_threads.empty() &&
                           !state.debugger_detected && state.convar_integrity.match &&
                           !vmt_violation;
  state.client_allowed_on_secure = state.b_secure_allowed && world.client_allowed_on_secure &&
                                   world.client_allowed_on_secure_servers;
  state.suppress_flag = world.silent_exclusion_active;
  if (!state.b_secure_allowed) state.violation_report = "CDllVerificationMonitor integrity violation";
  return state;
}

// Mirrors the secure-session decision after Message 159/157/160 collection. The
// scar is a rejected integrity signal; blue observes secure-session exclusion.
bool DllVerificationState::has_violations() const {
  return !b_secure_allowed || !client_allowed_on_secure || !suspicious_threads.empty() ||
         !convar_integrity.match || std::any_of(interface_vmts.begin(), interface_vmts.end(),
             [](const VmtEntry& entry) { return entry.anomalous; }) ||
         std::any_of(entity_vmts.begin(), entity_vmts.end(),
             [](const VmtEntry& entry) { return entry.anomalous; });
}

// Mirrors diagnostic logging around Message 159. It exposes red's artifacts as a
// concise report and shows blue which dimensions contributed to detection.
std::string DllVerificationState::describe() const {
  std::ostringstream output;
  output << "CDllVerificationMonitor pid=" << process_id << " modules=" << total_files_loaded
         << " trust_failures=" << files_need_trust_check << " suspicious_threads="
         << suspicious_threads.size() << " convar_match=" << convar_integrity.match
         << " secure_allowed=" << b_secure_allowed << " inventory_modules=" << module_count;
  if (!violation_report.empty()) output << " violation=" << violation_report;
  return output.str();
}

// Mirrors Message 159 field serialization. Red cannot remove the collected state
// without leaving tamper scars; blue receives the serialized flags for scoring.
std::string DllVerificationState::serialize_message_159() const {
  std::ostringstream output;
  output << "159{pid=" << process_id << ",active=" << is_active << ",focus=" << has_focus
         << ",files=" << total_files_loaded << ",trust_pending=" << files_need_trust_check
         << ",threads=" << thread_total_count << ",suspicious_threads=" << suspicious_threads.size()
         << ",debugger=" << debugger_detected << ",cpuid_vm=" << cpuid_vm_result
         << ",secure=" << b_secure_allowed << ",client_allowed=" << client_allowed_on_secure
         << ",client_ts=" << pe_timestamps.client_dll << ",cs2_ts=" << pe_timestamps.cs2_exe
         << ",kernel32_ts=" << pe_timestamps.kernel32 << ",ntdll_ts=" << pe_timestamps.ntdll
         << ",overlay_ts=" << pe_timestamps.gameoverlay << "}";
  return output.str();
}

// Mirrors Message 157's server-baseline CRC comparison. A modified ConVar list is
// the red artifact; blue detects mismatched CRCs/counts in the returned telemetry.
bool DllVerificationState::check_convar_integrity(const ConVarIntegrityResult& expected) {
  const bool counts_match = expected.count_all == 0 || expected.count_all == convar_integrity.count_all;
  const bool replicated_counts_match = expected.count_replicated == 0 ||
      expected.count_replicated == convar_integrity.count_replicated;
  convar_integrity.match = convar_integrity.match && expected.match &&
      convar_integrity.crc_all == expected.crc_all &&
      convar_integrity.crc_replicated == expected.crc_replicated && counts_match && replicated_counts_match;
  return convar_integrity.match;
}

// Mirrors Message 160's VMT and PE inventory pass. Hooked VMTs and malformed PE
// metadata are red scars; blue receives their counts, offsets, CRCs, and hashes.
void DllVerificationState::collect_inventory() {
  std::vector<uint8_t> vmt_bytes;
  for (const VmtEntry& entry : interface_vmts) append_u64(vmt_bytes, entry.address);
  for (const VmtEntry& entry : entity_vmts) append_u64(vmt_bytes, entry.address);
  vmt_crc = crc32(vmt_bytes.data(), vmt_bytes.size());

  std::vector<uint8_t> module_bytes;
  for (const PeModuleAnalysis& module : analyzed_modules) {
    const auto* hash_bytes = reinterpret_cast<const uint8_t*>(&module.name_hash);
    module_bytes.insert(module_bytes.end(), hash_bytes, hash_bytes + sizeof(module.name_hash));
    const auto* checksum_bytes = reinterpret_cast<const uint8_t*>(&module.checksum);
    module_bytes.insert(module_bytes.end(), checksum_bytes,
                        checksum_bytes + sizeof(module.checksum));
    module_bytes.insert(module_bytes.end(), std::begin(module.sha1_hash), std::end(module.sha1_hash));
  }
  module_crc = crc32(module_bytes.data(), module_bytes.size());
  module_count = static_cast<int>(analyzed_modules.size());
}

// Mirrors VAC's NtQuerySystemInformation(SystemHandleInformation) sweep. An RPM or
// query handle is the red scar; blue sees the owning process and requested rights.
std::vector<HandleTableEntry> scan_handle_table_for_game(const sim::World& world,
                                                          uint32_t game_pid) {
  std::vector<HandleTableEntry> entries;
  for (size_t index = 0; index < world.handles.size(); ++index) {
    const sim::Handle& handle = world.handles[index];
    if (handle.target_pid != game_pid || handle.owner_pid == game_pid ||
        (!sim::has(handle.access, sim::AccessMask::VmRead) &&
         !sim::has(handle.access, sim::AccessMask::Query))) continue;
    const sim::Process* owner = world.proc(handle.owner_pid);
    uint32_t access_mask = 0;
    if (sim::has(handle.access, sim::AccessMask::VmRead)) access_mask |= 0x0010u;
    if (sim::has(handle.access, sim::AccessMask::Query)) access_mask |= 0x0400u;
    entries.push_back({handle.owner_pid,
                       (static_cast<uint64_t>(handle.owner_pid) << 32) | (index + 1),
                       access_mask,
                       owner == nullptr ? "unknown" : owner->name});
  }
  return entries;
}

// Mirrors MonitorThreadContext start-address validation. RWX/private starts are
// red's injected-code scar; blue flags them before the diagnostic response is sent.
bool is_thread_start_suspicious(void* start_address, uint32_t protection,
                                void* module_handle) {
  const uintptr_t module = reinterpret_cast<uintptr_t>(module_handle);
  return start_address == nullptr || protection == kPageExecuteReadWrite ||
         module_handle == nullptr || module == kKernel32ModuleHandle ||
         module == kKernelBaseModuleHandle;
}

}  // namespace cs2
