// driver_loader.cpp — Complete kernel driver loading via SCM + module inventory.
// Educational: every operation documents what forensic artifact it leaves.
//
// LESSON: Loading a kernel driver creates artifacts in:
//   1. SCM database (services)
//   2. Registry (HKLM\SYSTEM\CurrentControlSet\Services)
//   3. Object Manager (\Driver\, \Device\)
//   4. Kernel module list (PsLoadedModuleList / SystemModuleInformation)
//   5. ETW/audit logs
// Blue anti-cheats scan ALL of these to detect unauthorized drivers.

#include "real/kernel/driver_loader.hpp"
#include "real/platform.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#  include "real/win/api_table.hpp"
#  include <psapi.h>
#  pragma comment(lib, "psapi.lib")
#  pragma comment(lib, "advapi32.lib")
#elif LR_PLATFORM_LINUX
#  include <dirent.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

namespace real::kernel {

// ── Pure helpers ───────────────────────────────────────────────────

static std::string lower_copy(const std::string& s) {
  std::string r = s;
  for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return r;
}

bool driver_name_matches(const std::string& module_name, const std::string& needle) {
  if (needle.empty()) return false;
  return lower_copy(module_name).find(lower_copy(needle)) != std::string::npos;
}

std::string driver_basename(const std::string& path_or_name) {
  if (path_or_name.empty()) return {};
  std::size_t slash = path_or_name.find_last_of("\\/");
  std::string base = (slash == std::string::npos) ? path_or_name
                                                  : path_or_name.substr(slash + 1);
  // Drop NT path prefix residue like "\SystemRoot\System32\drivers\foo.sys"
  return base;
}

// ── DriverLoadResult::describe ─────────────────────────────────────

std::string DriverLoadResult::describe() const {
  char buf[512];
  if (loaded) {
    std::snprintf(buf, sizeof(buf),
                  "Driver loaded: name=%s service=%s path=%s",
                  driver_name.c_str(), service_name.c_str(), image_path.c_str());
  } else {
    std::snprintf(buf, sizeof(buf),
                  "Driver NOT loaded: name=%s error=%llu detail=%s",
                  driver_name.c_str(),
                  static_cast<unsigned long long>(error_code),
                  detail.c_str());
  }
  return buf;
}

// ── load_driver ────────────────────────────────────────────────────

Result<DriverLoadResult> load_driver(const std::string& driver_path,
                                     const std::string& service_name,
                                     const std::string& display_name) {
  DriverLoadResult result;
  result.driver_name = driver_basename(driver_path);
  result.service_name = service_name;
  result.image_path = driver_path;

#ifndef NDEBUG
  std::printf("[kernel:driver] Loading driver: %s\n", driver_path.c_str());
  std::printf("[kernel:driver] Service name: %s\n", service_name.c_str());
#endif

#if LR_PLATFORM_WINDOWS
  auto& api = real::win::g_Api();
  if (!api.OpenSCManagerA || !api.CreateServiceA || !api.StartServiceA) {
    result.detail = "SCM APIs not resolved in ApiTable";
    result.error_code = 1;
    return result;
  }

  // ApiTable types SCM handles as HANDLE; cast to SC_HANDLE for SDK APIs.
  SC_HANDLE scm = static_cast<SC_HANDLE>(
      api.OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE));
  if (!scm) {
    result.detail = "OpenSCManager failed";
    result.error_code = GetLastError();
    return result;
  }

  SC_HANDLE svc = static_cast<SC_HANDLE>(api.CreateServiceA(
      scm,
      service_name.c_str(),
      display_name.empty() ? service_name.c_str() : display_name.c_str(),
      SERVICE_ALL_ACCESS,
      SERVICE_KERNEL_DRIVER,
      SERVICE_DEMAND_START,
      SERVICE_ERROR_NORMAL,
      driver_path.c_str(),
      nullptr, nullptr, nullptr, nullptr, nullptr));

  if (!svc) {
    DWORD err = GetLastError();
    if (err == ERROR_SERVICE_EXISTS) {
      if (api.OpenServiceA) {
        svc = static_cast<SC_HANDLE>(
            api.OpenServiceA(scm, service_name.c_str(), SERVICE_ALL_ACCESS));
      } else {
        svc = OpenServiceA(scm, service_name.c_str(), SERVICE_ALL_ACCESS);
      }
    }
    if (!svc) {
      result.detail = "CreateService/OpenService failed";
      result.error_code = GetLastError();
      CloseServiceHandle(scm);
      return result;
    }
  }

  if (!api.StartServiceA(svc, 0, nullptr)) {
    DWORD err = GetLastError();
    if (err != ERROR_SERVICE_ALREADY_RUNNING) {
      result.detail = "StartService failed";
      result.error_code = err;
      CloseServiceHandle(svc);
      CloseServiceHandle(scm);
      return result;
    }
  }

  SERVICE_STATUS_PROCESS ssp = {};
  DWORD bytes_needed = 0;
  if (QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO,
                           reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp),
                           &bytes_needed)) {
    result.loaded = (ssp.dwCurrentState == SERVICE_RUNNING);
    if (!result.loaded) {
      result.detail = "Service started but not RUNNING";
      result.error_code = ssp.dwCurrentState;
    } else {
      result.detail = "Driver loaded OK";
    }
  } else {
    // StartService succeeded; status query is best-effort.
    result.loaded = true;
    result.detail = "Driver loaded OK (status query unavailable)";
  }

  CloseServiceHandle(svc);
  CloseServiceHandle(scm);
  return result;

#elif LR_PLATFORM_LINUX
  // Educational: attempt finit_module / init_module via /sbin/insmod is
  // operator-driven; from-process load requires CAP_SYS_MODULE.
  (void)display_name;
  result.detail = "Linux kernel-module load requires insmod/modprobe with CAP_SYS_MODULE";
  result.error_code = static_cast<std::uint64_t>(EPERM);
  return result;
#else
  (void)display_name;
  result.detail = "Kernel driver loading is not supported on this platform";
  result.error_code = 1;
  return result;
#endif
}

// ── unload_driver ──────────────────────────────────────────────────

Result<void> unload_driver(const std::string& service_name) {
#ifndef NDEBUG
  std::printf("[kernel:driver] Unloading driver: %s\n", service_name.c_str());
#endif

#if LR_PLATFORM_WINDOWS
  auto& api = real::win::g_Api();
  if (!api.OpenSCManagerA) return os_error("OpenSCManager");

  SC_HANDLE scm = static_cast<SC_HANDLE>(
      api.OpenSCManagerA(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
  if (!scm) return os_error("OpenSCManager");

  SC_HANDLE svc = nullptr;
  if (api.OpenServiceA) {
    svc = static_cast<SC_HANDLE>(
        api.OpenServiceA(scm, service_name.c_str(), SERVICE_ALL_ACCESS));
  } else {
    svc = OpenServiceA(scm, service_name.c_str(), SERVICE_ALL_ACCESS);
  }
  if (!svc) {
    CloseServiceHandle(scm);
    return os_error("OpenService");
  }

  SERVICE_STATUS ss = {};
  if (api.ControlService) {
    api.ControlService(svc, SERVICE_CONTROL_STOP, &ss);
  } else {
    ControlService(svc, SERVICE_CONTROL_STOP, &ss);
  }

  bool deleted = false;
  if (api.DeleteService) {
    deleted = api.DeleteService(svc) != FALSE;
  } else {
    deleted = DeleteService(svc) != FALSE;
  }
  if (!deleted) {
    DWORD err = GetLastError();
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    if (err != ERROR_SERVICE_MARKED_FOR_DELETE) {
      return Result<void>("DeleteService failed: " + std::to_string(err));
    }
  }

  CloseServiceHandle(svc);
  CloseServiceHandle(scm);
  return Result<void>();

#elif LR_PLATFORM_LINUX
  (void)service_name;
  return Result<void>("Linux kernel-module unload requires rmmod with CAP_SYS_MODULE");
#else
  (void)service_name;
  return Result<void>("Driver unloading is not supported on this platform");
#endif
}

// ── driver_service_exists ──────────────────────────────────────────

Result<bool> driver_service_exists(const std::string& service_name) {
#if LR_PLATFORM_WINDOWS
  auto& api = real::win::g_Api();
  if (!api.OpenSCManagerA) return Result<bool>(false, "OpenSCManager not resolved");

  SC_HANDLE scm = static_cast<SC_HANDLE>(
      api.OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT));
  if (!scm) return Result<bool>(false, "OpenSCManager failed");

  SC_HANDLE svc = nullptr;
  if (api.OpenServiceA) {
    svc = static_cast<SC_HANDLE>(
        api.OpenServiceA(scm, service_name.c_str(), SERVICE_QUERY_STATUS));
  } else {
    svc = OpenServiceA(scm, service_name.c_str(), SERVICE_QUERY_STATUS);
  }
  bool exists = (svc != nullptr);
  if (svc) CloseServiceHandle(svc);
  CloseServiceHandle(scm);
  return Result<bool>(exists);

#elif LR_PLATFORM_LINUX
  // Treat service_name as a module name under /sys/module/<name>.
  std::string path = "/sys/module/" + service_name;
  struct stat st {};
  if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
    return Result<bool>(true);
  }
  return Result<bool>(false);
#else
  (void)service_name;
  return Result<bool>(false, "SCM requires Windows");
#endif
}

// ── enum_kernel_modules ────────────────────────────────────────────

#if LR_PLATFORM_WINDOWS
namespace {

// SystemModuleInformation = 11
constexpr ULONG kSystemModuleInformation = 11;

// SYSTEM_MODULE_INFORMATION entry — layout matches ntddk / ReactOS on x64.
// NumberOfModules is followed by 4 bytes of padding before the module array.
struct RtlProcessModuleInformation {
  PVOID Section;
  PVOID MappedBase;
  PVOID ImageBase;
  ULONG ImageSize;
  ULONG Flags;
  USHORT LoadOrderIndex;
  USHORT InitOrderIndex;
  USHORT LoadCount;
  USHORT OffsetToFileName;
  UCHAR FullPathName[256];
};
static_assert(sizeof(RtlProcessModuleInformation) == 296,
              "RTL_PROCESS_MODULE_INFORMATION size on x64");

Result<std::vector<KernelModuleInfo>> enum_via_system_module_info() {
  auto& api = real::win::g_Api();
  if (!api.NtQuerySystemInformation) {
    return Result<std::vector<KernelModuleInfo>>({}, "NtQuerySystemInformation not resolved");
  }

  ULONG needed = 0;
  NTSTATUS st = api.NtQuerySystemInformation(kSystemModuleInformation, nullptr, 0, &needed);
  // STATUS_INFO_LENGTH_MISMATCH = 0xC0000004
  if (needed == 0) {
    needed = 1 << 20;  // 1 MiB fallback
  }
  std::vector<std::uint8_t> buffer(needed + 0x1000);
  st = api.NtQuerySystemInformation(kSystemModuleInformation, buffer.data(),
                                    static_cast<ULONG>(buffer.size()), &needed);
  if (st < 0) {
    return Result<std::vector<KernelModuleInfo>>(
        {}, "NtQuerySystemInformation(SystemModuleInformation) failed: " +
                std::to_string(static_cast<long>(st)));
  }
  if (buffer.size() < 8) {
    return Result<std::vector<KernelModuleInfo>>({}, "SystemModuleInformation buffer too small");
  }

  ULONG count = 0;
  std::memcpy(&count, buffer.data(), sizeof(count));
  // Module array begins at offset 8 (ULONG count + 4-byte pad on x64).
  constexpr std::size_t kModsOffset = 8;
  if (buffer.size() < kModsOffset) {
    return Result<std::vector<KernelModuleInfo>>({}, "SystemModuleInformation truncated");
  }

  std::vector<KernelModuleInfo> out;
  out.reserve(count);
  for (ULONG i = 0; i < count; ++i) {
    const std::size_t off = kModsOffset + static_cast<std::size_t>(i) * sizeof(RtlProcessModuleInformation);
    if (off + sizeof(RtlProcessModuleInformation) > buffer.size()) break;
    RtlProcessModuleInformation m{};
    std::memcpy(&m, buffer.data() + off, sizeof(m));
    KernelModuleInfo info;
    info.base = reinterpret_cast<std::uint64_t>(m.ImageBase);
    info.size = static_cast<std::size_t>(m.ImageSize);
    info.path.assign(reinterpret_cast<const char*>(m.FullPathName));
    if (m.OffsetToFileName < sizeof(m.FullPathName)) {
      info.name = reinterpret_cast<const char*>(m.FullPathName + m.OffsetToFileName);
    } else {
      info.name = driver_basename(info.path);
    }
    out.push_back(std::move(info));
  }
  return out;
}

Result<std::vector<KernelModuleInfo>> enum_via_psapi() {
  std::vector<LPVOID> addrs(1024);
  DWORD needed = 0;
  if (!EnumDeviceDrivers(addrs.data(),
                         static_cast<DWORD>(addrs.size() * sizeof(LPVOID)),
                         &needed)) {
    return os_error("EnumDeviceDrivers");
  }
  if (needed > addrs.size() * sizeof(LPVOID)) {
    addrs.resize(needed / sizeof(LPVOID) + 8);
    if (!EnumDeviceDrivers(addrs.data(),
                           static_cast<DWORD>(addrs.size() * sizeof(LPVOID)),
                           &needed)) {
      return os_error("EnumDeviceDrivers resize");
    }
  }
  DWORD count = needed / static_cast<DWORD>(sizeof(LPVOID));
  std::vector<KernelModuleInfo> modules;
  modules.reserve(count);

  // Estimate sizes from sorted base gaps when ImageSize is unavailable.
  std::vector<std::uint64_t> bases;
  bases.reserve(count);
  for (DWORD i = 0; i < count; ++i) {
    bases.push_back(reinterpret_cast<std::uint64_t>(addrs[i]));
  }
  std::vector<std::uint64_t> sorted = bases;
  std::sort(sorted.begin(), sorted.end());

  for (DWORD i = 0; i < count; ++i) {
    KernelModuleInfo info;
    info.base = bases[i];
    char name_buf[MAX_PATH] = {};
    if (GetDeviceDriverBaseNameA(addrs[i], name_buf, sizeof(name_buf))) {
      info.name = name_buf;
    }
    char path_buf[MAX_PATH] = {};
    if (GetDeviceDriverFileNameA(addrs[i], path_buf, sizeof(path_buf))) {
      info.path = path_buf;
    }
    // Size from next higher base when available.
    auto it = std::upper_bound(sorted.begin(), sorted.end(), info.base);
    if (it != sorted.end() && *it > info.base) {
      info.size = static_cast<std::size_t>(*it - info.base);
      // Clamp absurd gaps (not adjacent modules).
      if (info.size > 64ull * 1024ull * 1024ull) info.size = 0;
    }
    modules.push_back(std::move(info));
  }
  return modules;
}

}  // namespace
#endif  // LR_PLATFORM_WINDOWS

Result<std::vector<KernelModuleInfo>> enum_kernel_modules() {
  std::vector<KernelModuleInfo> modules;

#if LR_PLATFORM_WINDOWS
  auto via_nt = enum_via_system_module_info();
  auto via_psapi = enum_via_psapi();

  // Merge both sources: PSAPI supplies reliable ImageBase addresses;
  // SystemModuleInformation supplies ImageSize + full path when available.
  if (via_psapi && !via_psapi->empty()) {
    std::vector<KernelModuleInfo> merged = *via_psapi;
    if (via_nt && !via_nt->empty()) {
      for (auto& m : merged) {
        const std::string pn = lower_copy(m.name);
        for (const auto& n : *via_nt) {
          const std::string nn = lower_copy(n.name);
          if (nn == pn || driver_name_matches(n.name, m.name) ||
              driver_name_matches(m.name, n.name)) {
            if (n.size != 0) m.size = n.size;
            if (!n.path.empty()) m.path = n.path;
            if (m.base == 0 && n.base != 0) m.base = n.base;
            break;
          }
        }
      }
    }
#ifndef NDEBUG
    std::printf("[kernel] Enumerated %zu kernel modules (PSAPI%s)\n",
                merged.size(),
                (via_nt && !via_nt->empty()) ? "+SystemModuleInformation" : "");
#endif
    return merged;
  }
  if (via_nt && !via_nt->empty()) {
#ifndef NDEBUG
    std::printf("[kernel] Enumerated %zu kernel modules (SystemModuleInformation)\n",
                via_nt->size());
#endif
    return via_nt;
  }
  if (!via_psapi) return via_psapi;
  return via_nt;

#elif LR_PLATFORM_LINUX
  FILE* f = std::fopen("/proc/modules", "r");
  if (!f) return os_error("fopen /proc/modules");

  char line[1024];
  while (std::fgets(line, sizeof(line), f)) {
    // Format: name size refcount deps state address [taint]
    // e.g. nf_nat 53248 3 nf_conntrack,xt_nat Live 0xffffffffc0123000
    char name[256] = {};
    unsigned long size = 0;
    int refcount = 0;
    char rest[512] = {};
    if (std::sscanf(line, "%255s %lu %d %511[^\n]", name, &size, &refcount, rest) < 3) {
      continue;
    }
    KernelModuleInfo info;
    info.name = name;
    info.size = static_cast<std::size_t>(size);

    // Optional address after "Live"/"Loading"/...
    const char* p = rest;
    while (*p) {
      if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        info.base = std::strtoull(p, nullptr, 16);
        break;
      }
      ++p;
    }
    info.path = std::string("/lib/modules/") + name + ".ko";
    modules.push_back(std::move(info));
  }
  std::fclose(f);
  return modules;

#else
  return Result<std::vector<KernelModuleInfo>>(
      {}, "Kernel module enumeration is not supported on this platform");
#endif
}

// ── is_driver_loaded ───────────────────────────────────────────────

Result<bool> is_driver_loaded(const std::string& driver_name) {
  auto modules = enum_kernel_modules();
  if (!modules) return Result<bool>(false, modules.error_msg);

  for (const auto& m : *modules) {
    if (driver_name_matches(m.name, driver_name) ||
        driver_name_matches(driver_basename(m.path), driver_name)) {
      return Result<bool>(true);
    }
  }
  return Result<bool>(false);
}

// ── get_driver_base ────────────────────────────────────────────────

Result<std::uint64_t> get_driver_base(const std::string& driver_name) {
  auto modules = enum_kernel_modules();
  if (!modules) return Result<std::uint64_t>(0, modules.error_msg);

  bool found = false;
  for (const auto& m : *modules) {
    if (driver_name_matches(m.name, driver_name) ||
        driver_name_matches(driver_basename(m.path), driver_name)) {
      found = true;
      // Prefer a non-zero base; hardened hosts may redact ImageBase to 0.
      if (m.base != 0) return Result<std::uint64_t>(m.base);
    }
  }
  if (found) return Result<std::uint64_t>(0);
  return Result<std::uint64_t>(0, "Driver not found: " + driver_name);
}

}  // namespace real::kernel
