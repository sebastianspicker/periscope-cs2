#include "lab/api_table.hpp"

#include <cstdint>
#include <functional>
#include <sstream>
#include <string>

namespace lab {
namespace {

constexpr ApiName kAllApis[] = {
    ApiName::NtOpenProcess,
    ApiName::NtReadVirtualMemory,
    ApiName::NtClose,
    ApiName::NtQuerySystemInformation,
    ApiName::OpenProcess,
    ApiName::ReadProcessMemory,
    ApiName::CloseHandle,
    ApiName::CreateWindowEx,
    ApiName::SetWindowPos,
    ApiName::SetLayeredWindowAttributes,
    ApiName::D3D11CreateDevice,
    ApiName::D3D11CreateDeviceAndSwapChain,
    ApiName::CreateDXGIFactory,
    ApiName::GetModuleHandleA,
    ApiName::GetProcAddress,
    ApiName::LoadLibraryA,
    ApiName::VirtualAllocEx,
    ApiName::WriteProcessMemory,
    ApiName::CreateRemoteThread,
    ApiName::SetWindowDisplayAffinity,
    ApiName::GetCurrentProcessId,
};

}  // namespace

ApiTable::ApiTable() { reset(); }

void ApiTable::reset() {
  entries_.clear();
  for (const auto api : kAllApis) {
    ApiEntry entry;
    entry.name = api;
    entry.module = std::string(module_for(api));
    entries_.emplace(api, std::move(entry));
  }
}

std::string_view ApiTable::to_string(ApiName name) {
  switch (name) {
    case ApiName::NtOpenProcess: return "NtOpenProcess";
    case ApiName::NtReadVirtualMemory: return "NtReadVirtualMemory";
    case ApiName::NtClose: return "NtClose";
    case ApiName::NtQuerySystemInformation: return "NtQuerySystemInformation";
    case ApiName::OpenProcess: return "OpenProcess";
    case ApiName::ReadProcessMemory: return "ReadProcessMemory";
    case ApiName::CloseHandle: return "CloseHandle";
    case ApiName::CreateWindowEx: return "CreateWindowExW";
    case ApiName::SetWindowPos: return "SetWindowPos";
    case ApiName::SetLayeredWindowAttributes: return "SetLayeredWindowAttributes";
    case ApiName::D3D11CreateDevice: return "D3D11CreateDevice";
    case ApiName::D3D11CreateDeviceAndSwapChain:
      return "D3D11CreateDeviceAndSwapChain";
    case ApiName::CreateDXGIFactory: return "CreateDXGIFactory1";
    case ApiName::GetModuleHandleA: return "GetModuleHandleA";
    case ApiName::GetProcAddress: return "GetProcAddress";
    case ApiName::LoadLibraryA: return "LoadLibraryA";
    case ApiName::VirtualAllocEx: return "VirtualAllocEx";
    case ApiName::WriteProcessMemory: return "WriteProcessMemory";
    case ApiName::CreateRemoteThread: return "CreateRemoteThread";
    case ApiName::SetWindowDisplayAffinity: return "SetWindowDisplayAffinity";
    case ApiName::GetCurrentProcessId: return "GetCurrentProcessId";
  }
  return "UnknownApi";
}

std::string_view ApiTable::module_for(ApiName name) {
  switch (name) {
    case ApiName::NtOpenProcess:
    case ApiName::NtReadVirtualMemory:
    case ApiName::NtClose:
    case ApiName::NtQuerySystemInformation:
      return "ntdll.dll";
    case ApiName::CreateWindowEx:
    case ApiName::SetWindowPos:
    case ApiName::SetLayeredWindowAttributes:
    case ApiName::SetWindowDisplayAffinity:
      return "user32.dll";
    case ApiName::D3D11CreateDevice:
    case ApiName::D3D11CreateDeviceAndSwapChain:
      return "d3d11.dll";
    case ApiName::CreateDXGIFactory:
      return "dxgi.dll";
    default:
      return "kernel32.dll";
  }
}

bool ApiTable::is_dangerous(ApiName name) {
  switch (name) {
    case ApiName::NtOpenProcess:
    case ApiName::NtReadVirtualMemory:
    case ApiName::OpenProcess:
    case ApiName::ReadProcessMemory:
    case ApiName::VirtualAllocEx:
    case ApiName::WriteProcessMemory:
    case ApiName::CreateRemoteThread:
      return true;
    default:
      return false;
  }
}

void* ApiTable::materialize_address(ApiName name) const {
  // Simulated module base + ordinal + per-build salt (not a real pointer).
  const auto module_tag = static_cast<std::uintptr_t>(
      std::hash<std::string_view>{}(module_for(name)) & 0x0FFFULL);
  const auto address = static_cast<std::uintptr_t>(0x7FFE0000ULL) +
                       (module_tag << 16) +
                       static_cast<std::uintptr_t>(name) * 0x10u +
                       static_cast<std::uintptr_t>(build_salt_ & 0x0FFFULL);
  return reinterpret_cast<void*>(address);
}

void* ApiTable::resolve(ApiName name) {
  return resolve_via(name, ApiResolvePath::PebEatWalk);
}

void* ApiTable::resolve_via(ApiName name, ApiResolvePath path) {
  if (path == ApiResolvePath::Unresolved) return nullptr;
  const auto it = entries_.find(name);
  if (it == entries_.end()) return nullptr;

  it->second.resolved = true;
  it->second.address = materialize_address(name);
  it->second.module = std::string(module_for(name));
  if (path == ApiResolvePath::ImportAddressTable) {
    it->second.from_iat = true;
    it->second.from_peb_walk = false;
  } else {
    it->second.from_iat = false;
    it->second.from_peb_walk = true;
  }
  return it->second.address;
}

int ApiTable::resolve_all(ApiResolvePath path) {
  int count = 0;
  for (const auto api : kAllApis) {
    if (resolve_via(api, path) != nullptr) ++count;
  }
  return count;
}

int ApiTable::resolve_dangerous(ApiResolvePath path) {
  int count = 0;
  for (const auto api : kAllApis) {
    if (!is_dangerous(api)) continue;
    if (resolve_via(api, path) != nullptr) ++count;
  }
  return count;
}

bool ApiTable::visible_in_iat(ApiName name) const {
  const auto it = entries_.find(name);
  return it != entries_.end() && it->second.from_iat;
}

bool ApiTable::is_resolved(ApiName name) const {
  const auto it = entries_.find(name);
  return it != entries_.end() && it->second.resolved;
}

std::vector<ApiEntry> ApiTable::resolved_apis() const {
  std::vector<ApiEntry> resolved;
  for (const auto& [_, entry] : entries_) {
    if (entry.resolved) resolved.push_back(entry);
  }
  return resolved;
}

std::vector<ApiEntry> ApiTable::all_entries() const {
  std::vector<ApiEntry> all;
  all.reserve(entries_.size());
  for (const auto& [_, entry] : entries_) all.push_back(entry);
  return all;
}

int ApiTable::resolved_count() const {
  int n = 0;
  for (const auto& [_, entry] : entries_) {
    if (entry.resolved) ++n;
  }
  return n;
}

void ApiTable::set_build_salt(std::uint64_t salt) { build_salt_ = salt; }

IatLeakReport ApiTable::analyze_iat_leaks() const {
  IatLeakReport report;
  for (const auto& [name, entry] : entries_) {
    if (!entry.resolved) continue;

    ++report.total_resolved;
    if (entry.from_iat) {
      ++report.in_iat;
      if (is_dangerous(name)) report.leaked.push_back(name);
    } else {
      ++report.runtime_resolved;
      if (is_dangerous(name)) report.hidden.push_back(name);
    }
  }

  if (report.total_resolved != 0) {
    report.stealth_score = static_cast<double>(report.runtime_resolved) /
                           static_cast<double>(report.total_resolved);
  }

  std::ostringstream oss;
  oss << "resolved=" << report.total_resolved << " iat=" << report.in_iat
      << " peb=" << report.runtime_resolved
      << " leaked_dangerous=" << report.leaked.size()
      << " hidden_dangerous=" << report.hidden.size()
      << " stealth=" << report.stealth_score;
  report.detail = oss.str();
  return report;
}

void ApiTable::apply_to_world(sim::World& world) const {
  const int n = resolved_count();
  world.dynamic_import_resolution = n > 0;
  world.dynamic_import_count = n;
  world.note("api_table apply_to_world resolved=" + std::to_string(n) +
             " stealth=" + std::to_string(analyze_iat_leaks().stealth_score));
}

}  // namespace lab
