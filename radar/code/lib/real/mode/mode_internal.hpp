// mode_internal.hpp — Shared probe helpers across mode_*.cpp TUs.
#pragma once

#include "real/mode/mode.hpp"
#include "real/platform.hpp"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace real::mode::detail {

inline bool ieq_ascii(const char* a, const char* b) {
  if (!a || !b) return false;
  while (*a && *b) {
    const unsigned char ca = static_cast<unsigned char>(*a++);
    const unsigned char cb = static_cast<unsigned char>(*b++);
    if (std::tolower(ca) != std::tolower(cb)) return false;
  }
  return *a == *b;
}

inline bool contains_ci(const char* hay, const char* needle) {
  if (!hay || !needle || !*needle) return false;
  const std::size_t nlen = std::strlen(needle);
  for (const char* p = hay; *p; ++p) {
    std::size_t i = 0;
    while (i < nlen) {
      const unsigned char a = static_cast<unsigned char>(p[i]);
      const unsigned char b = static_cast<unsigned char>(needle[i]);
      if (!a || std::tolower(a) != std::tolower(b)) break;
      ++i;
    }
    if (i == nlen) return true;
  }
  return false;
}

// Windows probes
#if LR_PLATFORM_WINDOWS
bool win_try_open_device(const char* path);
bool win_reg_key_exists(const wchar_t* subkey);
bool win_service_registered(const char* service_name);
bool win_token_elevated();
bool win_hwid_is_thunderbolt_or_usb4(const char* hwid, const char* desc);
bool win_hwid_is_fpga_dma_vendor(const char* hwid, const char* desc);
bool win_setupapi_dma_signals(std::string& reason);

extern const char* const kByovdDevicePaths[];
extern const char* const kByovdServiceNames[];
extern const char* const kDmaDevicePaths[];
// Counts for range-based for via sentinel? Prefer arrays with known sizes:
size_t byovd_device_path_count();
size_t byovd_service_name_count();
size_t dma_device_path_count();
#endif

// Linux probes
#if LR_PLATFORM_LINUX
bool linux_open_ro(const char* path);
bool linux_path_exists(const char* path);
bool linux_read_first_line(const char* path, std::string& out);
bool linux_pci_dma_signals(std::string& reason);
#endif

// CPUID / VMX (x64)
#if LR_ARCH_X64
void cpuid_leaf(std::uint32_t leaf, std::uint32_t* a, std::uint32_t* b,
                std::uint32_t* c, std::uint32_t* d);
bool cpu_has_vmx();
bool cpu_has_svm();
bool cpu_hypervisor_guest(std::string& vendor_out);
#endif

// Per-tier probe implementations (defined in mode.cpp)
TierProbe probe_t0();
TierProbe probe_t1();
TierProbe probe_t2();
TierProbe probe_t3();
TierProbe probe_t4();

}  // namespace real::mode::detail
