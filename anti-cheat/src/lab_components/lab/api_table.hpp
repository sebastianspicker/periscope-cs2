#pragma once

// Educational simulation: runtime API resolution table. Models API access
// through a PEB walk rather than static import-table entries.

#include "sim/world.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace lab {

enum class ApiName : std::uint16_t {
  NtOpenProcess,
  NtReadVirtualMemory,
  NtClose,
  NtQuerySystemInformation,
  OpenProcess,
  ReadProcessMemory,
  CloseHandle,
  CreateWindowEx,
  SetWindowPos,
  SetLayeredWindowAttributes,
  D3D11CreateDevice,
  D3D11CreateDeviceAndSwapChain,
  CreateDXGIFactory,
  GetModuleHandleA,
  GetProcAddress,
  LoadLibraryA,
  VirtualAllocEx,
  WriteProcessMemory,
  CreateRemoteThread,
  SetWindowDisplayAffinity,
  GetCurrentProcessId,
};

enum class ApiResolvePath : std::uint8_t {
  Unresolved = 0,
  ImportAddressTable,  // static IAT — visible to import scanners
  PebEatWalk,          // runtime PEB+EAT — preferred stealth path
};

struct ApiEntry {
  ApiName name;
  bool resolved = false;
  bool from_iat = false;
  bool from_peb_walk = false;
  void* address = nullptr;
  std::string module;  // ntdll / kernel32 / user32 / d3d11 / dxgi
};

struct IatLeakReport {
  int total_resolved = 0;
  int in_iat = 0;
  int runtime_resolved = 0;
  std::vector<ApiName> leaked;
  std::vector<ApiName> hidden;
  double stealth_score = 1.0;
  std::string detail;
};

class ApiTable {
 public:
  using IatLeakReport = lab::IatLeakReport;

  ApiTable();

  // Resolve via PEB+EAT walk (default stealth path).
  void* resolve(ApiName name);

  // Explicit path selection for lessons.
  void* resolve_via(ApiName name, ApiResolvePath path);

  // Resolve the full catalog; returns count of newly resolved entries.
  int resolve_all(ApiResolvePath path = ApiResolvePath::PebEatWalk);

  // Resolve only the process-memory / injection surface APIs.
  int resolve_dangerous(ApiResolvePath path = ApiResolvePath::PebEatWalk);

  bool visible_in_iat(ApiName name) const;
  bool is_resolved(ApiName name) const;
  std::vector<ApiEntry> resolved_apis() const;
  std::vector<ApiEntry> all_entries() const;
  int resolved_count() const;

  void set_build_salt(std::uint64_t salt);
  std::uint64_t build_salt() const { return build_salt_; }

  IatLeakReport analyze_iat_leaks() const;

  // Plant sim::World scars used by dynamic_api_table strategies.
  void apply_to_world(sim::World& world) const;

  static std::string_view to_string(ApiName name);
  static std::string_view module_for(ApiName name);
  static bool is_dangerous(ApiName name);

  void reset();

 private:
  void* materialize_address(ApiName name) const;

  std::map<ApiName, ApiEntry> entries_;
  std::uint64_t build_salt_ = 0;
};

}  // namespace lab
