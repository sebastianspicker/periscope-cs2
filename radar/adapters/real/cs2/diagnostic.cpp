// EDUCATIONAL NOTE: The diagnostic offsets below are CS2 build-specific.
// They are resolved at runtime via signature scanning in offsets.cpp.
// If your CS2 build is newer than this code, these offsets will need
// updating. This is expected — CS2 changes offsets every update.
//
// Honesty contract: never invent fixed success (e.g. fake thread counts).
// Reads that cannot be proven from process memory or OS query fail with
// a non-empty error — consumers treat that as unavailable telemetry.

#include "real/cs2/diagnostic.hpp"
#include "real/cs2/offsets_snapshot.hpp"
#include "real/win/xorstr.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <utility>
#include <vector>

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#include "real/win/api_table.hpp"
#endif

namespace real::cs2 {
namespace {

// Helper: read from Cs2MemoryReader and return bytes as vector on success
std::vector<uint8_t> do_read(Cs2MemoryReader& reader, uint64_t addr, size_t size) {
  auto r = reader.read(addr, size);
  if (r.status != ac::Status::Ok) return {};
  return r.bytes;
}

// Recover client.dll base from absolute global addresses (entity_list RVA).
std::uint64_t client_base_from_offsets(const Cs2Offsets& offsets) {
  if (offsets.entity_list > snapshot::globals::dwEntityList) {
    return offsets.entity_list - snapshot::globals::dwEntityList;
  }
  if (offsets.local_player > snapshot::globals::dwLocalPlayerPawn) {
    return offsets.local_player - snapshot::globals::dwLocalPlayerPawn;
  }
  return 0;
}

// Read PE TimeDateStamp from a mapped module base in the remote process.
bool read_pe_timestamp(Cs2MemoryReader& reader, std::uint64_t module_base,
                       std::uint32_t& out_ts) {
  if (!module_base) return false;
  std::uint8_t dos[0x40]{};
  auto d = do_read(reader, module_base, sizeof(dos));
  if (d.size() != sizeof(dos) || d[0] != 'M' || d[1] != 'Z') return false;
  std::int32_t e_lfanew = 0;
  std::memcpy(&e_lfanew, d.data() + 0x3C, 4);
  if (e_lfanew <= 0 || e_lfanew > 0x1000) return false;
  // COFF FileHeader.TimeDateStamp is at e_lfanew + 8 (after PE sig + 4).
  auto ts_bytes =
      do_read(reader, module_base + static_cast<std::uint64_t>(e_lfanew) + 8,
              sizeof(std::uint32_t));
  if (ts_bytes.size() != sizeof(std::uint32_t)) return false;
  std::memcpy(&out_ts, ts_bytes.data(), sizeof(out_ts));
  return out_ts != 0;
}

#if LR_PLATFORM_WINDOWS
// SYSTEM_PROCESS_INFORMATION layout (partial) — same shape as process_cache.
struct DiagSystemProcessInfo {
  ULONG NextEntryOffset;
  ULONG NumberOfThreads;
  BYTE Reserved1[48];
  UNICODE_STRING ImageName;
  LONG BasePriority;
  HANDLE UniqueProcessId;
  PVOID InheritedFromUniqueProcessId;
};

// Honest OS query: SystemProcessInformation NumberOfThreads for cs2.exe.
// Returns 0 when the process is absent or NtQSI is unavailable — never fakes.
int count_cs2_threads_via_ntqsi() {
  auto& api = real::win::g_Api();
  if (!api.resolved || !api.NtQuerySystemInformation) return 0;

  ULONG bufSize = 0x10000;
  std::vector<std::uint8_t> buffer(bufSize);
  NTSTATUS status = 0;
  while ((status = api.NtQuerySystemInformation(5 /*SystemProcessInformation*/,
                                                buffer.data(), bufSize,
                                                &bufSize)) < 0) {
    if (bufSize > 0x200000) return 0;
    buffer.resize(bufSize);
  }

  std::uint8_t* pos = buffer.data();
  while (pos) {
    auto* info = reinterpret_cast<DiagSystemProcessInfo*>(pos);
    if (info->ImageName.Buffer && info->ImageName.Length > 0) {
      const int wideLen =
          info->ImageName.Length / static_cast<int>(sizeof(wchar_t));
      char nameBuf[64]{};
      for (int i = 0; i < wideLen && i < 63; ++i) {
        const wchar_t c = info->ImageName.Buffer[i];
        nameBuf[i] = (c >= L'A' && c <= L'Z')
                         ? static_cast<char>(c - L'A' + 'a')
                         : static_cast<char>(c < 128 ? c : '?');
      }
      if (std::strcmp(nameBuf, "cs2.exe") == 0) {
        const int n = static_cast<int>(info->NumberOfThreads);
        return n > 0 ? n : 0;
      }
    }
    if (info->NextEntryOffset == 0) break;
    pos += info->NextEntryOffset;
  }
  return 0;
}
#endif

}  // namespace

std::string RealDiagnosticState::describe() const {
  std::ostringstream out;
  out << "verified_modules=" << verified_module_count
      << " thread_count=" << thread_count
      << " debugger_detected=" << (debugger_detected ? "true" : "false")
      << " secure_allowed=" << (client_allowed_on_secure ? "true" : "false");
  return out.str();
}

Result<std::vector<std::string>> read_verified_modules(Cs2MemoryReader& reader,
                                                       const Cs2Offsets& offsets) {
#if LR_PLATFORM_WINDOWS
  if (!reader.is_attached()) {
    return {{}, "Reader not attached — cannot verify modules"};
  }

  std::vector<std::string> modules;
  const std::uint64_t client_base = client_base_from_offsets(offsets);

  // Primary path: prove client.dll is mapped by reading PE header + timestamp.
  if (client_base != 0) {
    std::uint32_t ts = 0;
    if (read_pe_timestamp(reader, client_base, ts)) {
      modules.emplace_back(OBF("client.dll"));
    }
  }

  // Secondary: scan known absolute globals' nearby string tables for module names.
  const uint64_t probe_addrs[] = {
      offsets.radar_base != 0 ? offsets.radar_base : 0,
      offsets.view_angles != 0 ? offsets.view_angles - 0x200 : 0,
      offsets.entity_list != 0 ? offsets.entity_list - 0x1000 : 0,
      client_base,
  };

  const char* known_modules[] = {
      OBF("client.dll"), OBF("engine2.dll"), OBF("tier0.dll"),
      OBF("inputsystem.dll"), OBF("schemasystem.dll"), OBF("studiorender.dll")};

  for (auto probe_addr : probe_addrs) {
    if (probe_addr == 0) continue;
    auto buf_vec = do_read(reader, probe_addr, 128);
    if (buf_vec.size() < 16) continue;
    for (auto mod_name : known_modules) {
      const size_t mod_len = std::strlen(mod_name);
      if (mod_len == 0 || mod_len >= buf_vec.size()) continue;
      for (size_t off = 0; off + mod_len < buf_vec.size(); ++off) {
        if (std::memcmp(buf_vec.data() + off, mod_name, mod_len) == 0) {
          if (std::find(modules.begin(), modules.end(), mod_name) ==
              modules.end()) {
            modules.emplace_back(mod_name);
          }
        }
      }
    }
  }

  if (!modules.empty()) {
    return Result<std::vector<std::string>>(std::move(modules));
  }

  // Honest failure — no invented module list.
  return {{}, "No diagnostic module data readable (attach + offsets required)"};
#else
  (void)reader;
  (void)offsets;
  return {{}, "Real CS2 diagnostic reads are supported only on Windows"};
#endif
}

Result<int> read_thread_count(Cs2MemoryReader& reader, const Cs2Offsets& offsets) {
  (void)offsets;
#if LR_PLATFORM_WINDOWS
  if (!reader.is_attached()) {
    return {0, "Reader not attached — cannot count threads"};
  }

  // Prefer OS-level NtQuerySystemInformation NumberOfThreads for cs2.exe.
  const int os_count = count_cs2_threads_via_ntqsi();
  if (os_count > 0) return Result<int>(os_count);

  // Fallback: probe memory only if a value in a sane thread-count range is found.
  // Never invent a fixed count on failure.
  int thread_count = 0;
  const uint64_t probe_offsets[] = {
      0x100, 0x104, 0x108, 0x10C, 0x200, 0x204, 0x208, 0x20C,
  };
  if (offsets.radar_base != 0) {
    for (auto off : probe_offsets) {
      auto val_bytes = do_read(reader, offsets.radar_base + off, sizeof(int));
      if (val_bytes.size() == sizeof(int)) {
        int val = 0;
        std::memcpy(&val, val_bytes.data(), sizeof(int));
        if (val >= 1 && val <= 256) {
          thread_count = val;
          break;
        }
      }
    }
  }

  if (thread_count > 0) {
    return Result<int>(thread_count);
  }

  return {0, "Thread count unavailable (cs2.exe not in process list and no memory probe hit)"};
#else
  (void)reader;
  (void)offsets;
  return {0, "Real CS2 diagnostic reads are supported only on Windows"};
#endif
}

Result<RealDiagnosticState> read_diagnostic_state(Cs2MemoryReader& reader,
                                                   const Cs2Offsets& offsets) {
  RealDiagnosticState state;
#if LR_PLATFORM_WINDOWS
  if (!reader.is_attached()) {
    return {state, "Reader not attached"};
  }

  // Debugger presence on the local process (radar host) — real OS query.
  // Direct call is educational (IAT scar); mirrors what CS2 itself checks.
  state.debugger_detected = ::IsDebuggerPresent() != FALSE;

  // PE timestamps from client.dll when base is recoverable.
  const std::uint64_t client_base = client_base_from_offsets(offsets);
  if (client_base) {
    std::uint32_t ts = 0;
    if (read_pe_timestamp(reader, client_base, ts)) {
      state.pe_timestamp_client_dll = ts;
    }
  }

  const auto modules = read_verified_modules(reader, offsets);
  if (modules) {
    state.verified_modules = *modules;
    state.verified_module_count = static_cast<int>(state.verified_modules.size());
  }

  const auto threads = read_thread_count(reader, offsets);
  if (threads) {
    state.thread_count = *threads;
    state.thread_captured = state.thread_count > 0;
  }

  // Partial success is OK when at least one real signal was collected.
  if (state.verified_module_count == 0 && !state.thread_captured &&
      state.pe_timestamp_client_dll == 0) {
    return {state,
            modules ? modules.error_msg.c_str()
                    : (threads ? threads.error_msg.c_str()
                               : "No diagnostic signals readable")};
  }
  return Result<RealDiagnosticState>(std::move(state));
#else
  (void)reader;
  (void)offsets;
  return {state, "Real CS2 diagnostic reads are supported only on Windows"};
#endif
}

}  // namespace real::cs2
