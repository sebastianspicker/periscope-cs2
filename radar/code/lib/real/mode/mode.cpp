// mode.cpp — Runtime mode and tier availability (public API + probes).
//
// Platform probe helpers live in mode_win_probes / mode_linux_probes /
// mode_cpu_vmx. This TU owns parse_mode, RuntimeConfig, and probe_tier.

#include "real/mode/mode_internal.hpp"
#include "real/cs2/process.hpp"
#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#include "real/win/api_table.hpp"
#include "real/win/xorstr.hpp"
#elif LR_PLATFORM_LINUX
#ifndef OBF
#define OBF(s) (s)
#endif
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace real::mode {
namespace detail {

TierProbe probe_t0() {
  TierProbe p;
  p.tier = Tier::T0_UsermodeRpm;

  auto result = real::cs2::find_cs2_process();
  if (!result) {
    p.available = false;
    p.reason = result.error_msg.empty() ? "CS2 process not found"
                                        : result.error_msg.c_str();
    return p;
  }

#if LR_PLATFORM_WINDOWS
  auto& api = real::win::g_Api();
  api.ensure_resolved();
  if (!api.NtOpenProcess || !api.NtClose) {
    p.available = false;
    p.reason = "NtOpenProcess unavailable (API table unresolved)";
    return p;
  }
  HANDLE h = nullptr;
  CLIENT_ID cid{};
  cid.UniqueProcess = reinterpret_cast<void*>(
      static_cast<uintptr_t>((*result).pid));
  cid.UniqueThread = nullptr;
  OBJECT_ATTRIBUTES oa{};
  oa.Length = sizeof(oa);
  const NTSTATUS status =
      api.NtOpenProcess(&h, PROCESS_QUERY_LIMITED_INFORMATION, &oa, &cid);
  if (status < 0 || !h) {
    p.available = false;
    char buf[128];
    std::snprintf(buf, sizeof(buf),
                  "CS2 pid=%u found but NtOpenProcess failed (status=0x%08lX)",
                  (*result).pid, static_cast<unsigned long>(status));
    p.reason = buf;
    return p;
  }
  api.NtClose(h);
  char buf[96];
  std::snprintf(buf, sizeof(buf),
                "CS2 pid=%u openable with PROCESS_QUERY_LIMITED_INFORMATION",
                (*result).pid);
  p.available = true;
  p.reason = buf;
#elif LR_PLATFORM_LINUX
  char path[64];
  std::snprintf(path, sizeof(path), "/proc/%u", (*result).pid);
  struct stat st{};
  if (::stat(path, &st) != 0) {
    p.available = false;
    p.reason = std::string("CS2 pid found but ") + path + " not accessible";
    return p;
  }
  char buf[96];
  std::snprintf(buf, sizeof(buf), "CS2 pid=%u /proc entry accessible",
                (*result).pid);
  p.available = true;
  p.reason = buf;
#else
  p.available = false;
  p.reason = "T0 process attach not supported on this platform";
#endif
  return p;
}

TierProbe probe_t1() {
  TierProbe p;
  p.tier = Tier::T1_Syscall;

#if !LR_ARCH_X64
  p.available = false;
  p.reason = "direct syscall tier requires x86-64";
  return p;
#endif

#if LR_PLATFORM_WINDOWS
  auto& api = real::win::g_Api();
  api.ensure_resolved();
  if (api.NtClose && api.NtDeviceIoControlFile && api.NtQuerySystemInformation) {
    p.available = true;
    p.reason = "x64 + ntdll NT exports resolved (direct syscall capable)";
  } else {
    p.available = false;
    p.reason = "ntdll NT exports not resolved for syscall path";
  }
#elif LR_PLATFORM_LINUX
  p.available = true;
  p.reason = "x64 Linux syscall instruction available";
#else
  p.available = false;
  p.reason = "syscall tier unsupported on this platform";
#endif
  return p;
}

TierProbe probe_t2() {
  TierProbe p;
  p.tier = Tier::T2_BYOVD;

#if LR_PLATFORM_LINUX
  if (linux_open_ro("/dev/mem")) {
    p.available = true;
    p.reason = "/dev/mem openable (physmem path)";
    return p;
  }
  if (linux_open_ro("/dev/crash")) {
    p.available = true;
    p.reason = "/dev/crash openable (physmem path)";
    return p;
  }
  if (linux_open_ro("/dev/kmem")) {
    p.available = true;
    p.reason = "/dev/kmem openable";
    return p;
  }
  static const char* const kNodes[] = {
      "/dev/legit_radar_phys", "/dev/winring0", "/dev/gdrv"};
  for (const char* n : kNodes) {
    if (linux_open_ro(n)) {
      p.available = true;
      p.reason = std::string("kernel device node openable: ") + n;
      return p;
    }
  }
  p.available = false;
  p.reason = "no /dev/mem, /dev/crash, or research physmem device openable";
  return p;

#elif LR_PLATFORM_WINDOWS
  if (config().driver_device_path &&
      win_try_open_device(config().driver_device_path)) {
    p.available = true;
    p.reason = std::string("configured driver device openable: ") +
               config().driver_device_path;
    return p;
  }

  for (size_t i = 0; i < byovd_device_path_count(); ++i) {
    const char* path = kByovdDevicePaths[i];
    if (win_try_open_device(path)) {
      p.available = true;
      p.reason = std::string("kernel/BYOVD device openable: ") + path;
      return p;
    }
  }

  for (size_t i = 0; i < byovd_service_name_count(); ++i) {
    const char* svc = kByovdServiceNames[i];
    if (win_service_registered(svc)) {
      p.available = true;
      p.reason = std::string("BYOVD-class service registered: ") + svc;
      return p;
    }
  }

  if (win_token_elevated()) {
    p.available = false;
    p.reason =
        "elevated token present but no BYOVD device/service/physmem path openable";
  } else {
    p.available = false;
    p.reason = "no BYOVD device, physmem node, or known vulnerable service";
  }
  return p;

#else
  p.available = false;
  p.reason = "T2 kernel path unsupported on this platform";
  return p;
#endif
}

TierProbe probe_t3() {
  TierProbe p;
  p.tier = Tier::T3_Hypervisor;

#if LR_ARCH_X64
  const bool vmx = cpu_has_vmx();
  const bool svm = cpu_has_svm();
  if (vmx || svm) {
    p.available = true;
    if (vmx && svm) {
      p.reason = "CPUID reports both VMX and SVM";
    } else if (vmx) {
      p.reason = "CPUID reports Intel VT-x (VMX)";
    } else {
      p.reason = "CPUID reports AMD-V (SVM)";
    }
    std::string hv_vendor;
    if (cpu_hypervisor_guest(hv_vendor)) {
      p.reason += " (currently a hypervisor guest";
      if (!hv_vendor.empty()) {
        p.reason += ": ";
        p.reason += hv_vendor;
      }
      p.reason += ")";
    }
  } else {
    p.available = false;
    p.reason = "CPUID reports neither VMX nor SVM";
  }
#else
  p.available = false;
  p.reason = "hypervisor tier requires x86-64 CPUID";
#endif
  return p;
}

TierProbe probe_t4() {
  TierProbe p;
  p.tier = Tier::T4_DMA;

#if LR_PLATFORM_LINUX
  std::string reason;
  if (linux_pci_dma_signals(reason)) {
    p.available = true;
    p.reason = reason;
  } else {
    p.available = false;
    p.reason = reason;
  }
  return p;

#elif LR_PLATFORM_WINDOWS
  static const wchar_t* const kRegKeys[] = {
      L"SYSTEM\\CurrentControlSet\\Services\\ThunderboltService",
      L"SYSTEM\\CurrentControlSet\\Services\\Thunderbolt",
      L"SYSTEM\\CurrentControlSet\\Services\\Usb4HostRouter",
      L"SYSTEM\\CurrentControlSet\\Services\\USB4HOSTROUTER",
  };
  for (const wchar_t* key : kRegKeys) {
    if (win_reg_key_exists(key)) {
      p.available = true;
      p.reason = "Thunderbolt/USB4 service key present in registry";
      return p;
    }
  }

  for (size_t i = 0; i < dma_device_path_count(); ++i) {
    const char* path = kDmaDevicePaths[i];
    if (win_try_open_device(path)) {
      p.available = true;
      p.reason = std::string("DMA device openable: ") + path;
      return p;
    }
  }

  std::string reason;
  if (win_setupapi_dma_signals(reason)) {
    p.available = true;
    p.reason = reason;
    return p;
  }

  p.available = false;
  p.reason = reason.empty()
                 ? "no Thunderbolt/USB4/FPGA/DMA device signals"
                 : reason;
  return p;

#else
  p.available = false;
  p.reason = "T4 DMA detection unsupported on this platform";
  return p;
#endif
}

}  // namespace detail

// ═══════════════════════════════════════════════════════════════════════
// Public config / mode control
// ═══════════════════════════════════════════════════════════════════════

const char* default_cs2_name() {
#if LR_PLATFORM_WINDOWS
  return OBF("cs2.exe");
#else
  return "cs2";
#endif
}

bool parse_mode(const char* text, Mode& out) {
  if (!text || !*text) return false;
  if (detail::ieq_ascii(text, "real")) {
    out = Mode::Real;
    return true;
  }
  if (detail::ieq_ascii(text, "sim")) {
    out = Mode::Sim;
    return true;
  }
  if (detail::ieq_ascii(text, "hybrid")) {
    out = Mode::Hybrid;
    return true;
  }
  return false;
}

void RuntimeConfig::load_from_env() {
  const char* env_mode = std::getenv("LR_MODE");
  if (env_mode && *env_mode) {
    Mode parsed = Mode::Hybrid;
    if (parse_mode(env_mode, parsed)) {
      mode = parsed;
    } else {
      std::printf("[mode] Warning: unknown LR_MODE=\"%s\", keeping %s\n",
                  env_mode, mode_name(mode));
    }
  }

  const char* env_verbose = std::getenv("LR_VERBOSE");
  if (env_verbose && *env_verbose) {
    verbose = detail::ieq_ascii(env_verbose, "1") ||
              detail::ieq_ascii(env_verbose, "true") ||
              detail::ieq_ascii(env_verbose, "yes") ||
              detail::ieq_ascii(env_verbose, "on");
    if (detail::ieq_ascii(env_verbose, "0") ||
        detail::ieq_ascii(env_verbose, "false") ||
        detail::ieq_ascii(env_verbose, "no") ||
        detail::ieq_ascii(env_verbose, "off")) {
      verbose = false;
    }
  }

  const char* env_drv = std::getenv("LR_DRV_PATH");
  if (env_drv && *env_drv) {
    driver_device_path = env_drv;
  }
}

void set_mode(Mode m) {
  config().mode = m;
  if (config().verbose) {
    std::printf("[mode] Runtime mode set to: %s\n", mode_name(m));
  }
}

bool cs2_available() {
  auto result = real::cs2::find_cs2_process();
  if (result) {
#ifndef NDEBUG
    if (config().verbose) {
      std::printf("[mode] CS2 process found: pid=%u name=%s\n",
                  (*result).pid, (*result).name.c_str());
    }
#endif
    return true;
  }
#ifndef NDEBUG
  if (config().verbose) {
    std::printf("[mode] CS2 process not found: %s\n",
                result.error_msg.c_str());
  }
#endif
  return false;
}

TierProbe probe_tier(Tier t) {
  switch (t) {
    case Tier::T0_UsermodeRpm: return detail::probe_t0();
    case Tier::T1_Syscall:     return detail::probe_t1();
    case Tier::T2_BYOVD:       return detail::probe_t2();
    case Tier::T3_Hypervisor:  return detail::probe_t3();
    case Tier::T4_DMA:         return detail::probe_t4();
    default: {
      TierProbe p;
      p.tier = t;
      p.available = false;
      p.reason = "unknown tier";
      return p;
    }
  }
}

bool tier_available(Tier t) {
  return probe_tier(t).available;
}

std::vector<TierProbe> probe_all_tiers() {
  std::vector<TierProbe> out;
  out.reserve(static_cast<std::size_t>(Tier::Count));
  for (int i = 0; i < static_cast<int>(Tier::Count); ++i) {
    out.push_back(probe_tier(static_cast<Tier>(i)));
  }
  return out;
}

std::uint8_t available_tiers_mask() {
  std::uint8_t mask = 0;
  for (int i = 0; i < static_cast<int>(Tier::Count); ++i) {
    if (tier_available(static_cast<Tier>(i))) {
      mask = static_cast<std::uint8_t>(mask | (1u << i));
    }
  }
  return mask;
}

int highest_available_tier() {
  for (int i = static_cast<int>(Tier::Count) - 1; i >= 0; --i) {
    if (tier_available(static_cast<Tier>(i))) return i;
  }
  return -1;
}

std::string describe_mode() {
  const auto& cfg = config();
  const char* env_mode = std::getenv("LR_MODE");
  char buf[768];
  std::snprintf(
      buf, sizeof(buf),
      "Runtime mode: %s\n"
      "  may_use_real: %s\n"
      "  may_use_sim: %s\n"
      "  CS2 available: %s\n"
      "  Verbose: %s\n"
      "  Driver path: %s\n"
      "  Env: LR_MODE=%s\n",
      mode_name(cfg.mode),
      cfg.may_use_real() ? "yes" : "no",
      cfg.may_use_sim() ? "yes" : "no",
      cs2_available() ? "yes" : "no",
      cfg.verbose ? "yes" : "no",
      cfg.driver_device_path ? cfg.driver_device_path : "(default)",
      env_mode ? env_mode : "(unset)");
  return buf;
}

std::string describe_tier_availability() {
  char buf[2048];
  int offset = 0;
  const int cap = static_cast<int>(sizeof(buf));

  offset += std::snprintf(buf + offset, static_cast<std::size_t>(cap - offset),
                          "Tier availability:\n");

  for (int t = 0; t < static_cast<int>(Tier::Count); ++t) {
    if (offset >= cap - 1) break;
    const TierProbe probe = probe_tier(static_cast<Tier>(t));
    offset += std::snprintf(
        buf + offset, static_cast<std::size_t>(cap - offset),
        "  %-6s %-25s %-3s  (%s)\n", tier_short_name(probe.tier),
        tier_name(probe.tier), probe.available ? "yes" : "no",
        probe.reason.c_str());
  }

  const int highest = highest_available_tier();
  if (offset < cap - 1) {
    if (highest < 0) {
      offset += std::snprintf(buf + offset,
                              static_cast<std::size_t>(cap - offset),
                              "  Highest available: none\n");
    } else {
      offset += std::snprintf(buf + offset,
                              static_cast<std::size_t>(cap - offset),
                              "  Highest available: T%d\n", highest);
    }
  }
  (void)offset;
  return buf;
}

}  // namespace real::mode
