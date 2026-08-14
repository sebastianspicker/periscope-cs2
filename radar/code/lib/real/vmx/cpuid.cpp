// CPUID and timing helpers for VMX diagnostics (fully unprivileged).
#include "real/vmx/vmx_intrin.hpp"
#include "real/vmx/vmx_priv.hpp"

#if LR_ARCH_X64

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>

#if LR_COMPILER_MSVC
#  include <intrin.h>
#endif

namespace real::vmx {
namespace {

#if LR_COMPILER_GCC || LR_COMPILER_CLANG
static inline uint64_t rdtsc_serial() {
  unsigned int aux;
  return __builtin_ia32_rdtscp(&aux);
}
#else
static inline uint64_t rdtsc_serial() {
  unsigned int aux;
  return __rdtscp(&aux);
}
#endif

/// Calibrate CPUID latency: 100 samples, median.
static uint64_t calibrate_cpuid_latency() {
  uint64_t samples[100];
  for (int i = 0; i < 100; ++i) {
    uint64_t start = rdtsc_serial();
    uint32_t a = 0, b = 0, c = 0, d = 0;
    detail::cpuid_leaf(0, &a, &b, &c, &d);
    samples[i] = rdtsc_serial() - start;
  }
  std::sort(samples, samples + 100);
  return samples[50];
}

}  // namespace

Result<bool> vmx_supported() {
  uint32_t a = 0, b = 0, c = 0, d = 0;
  detail::cpuid_leaf(1, &a, &b, &c, &d);
  return Result<bool>((c & (1U << 5)) != 0);
}

Result<bool> svm_supported() {
  uint32_t a = 0, b = 0, c = 0, d = 0;
  detail::cpuid_leaf(0x80000000, &a, &b, &c, &d);
  if (a < 0x80000001) return Result<bool>(false);
  detail::cpuid_leaf(0x80000001, &a, &b, &c, &d);
  return Result<bool>((c & (1U << 2)) != 0);
}

/// Returns a calibrated baseline CPUID latency value (2x median) for timing spoofing.
uint64_t get_cpuid_latency() {
  static uint64_t calibrated = calibrate_cpuid_latency();
  return calibrated * 2;
}

CpuidDump dump_cpuid() {
  CpuidDump dump;
  uint32_t a = 0, b = 0, c = 0, d = 0;
  detail::cpuid_leaf(0, &a, &b, &c, &d);
  dump.max_leaf = a;
  char vendor[13] = {};
  std::memcpy(vendor, &b, 4);
  std::memcpy(vendor + 4, &d, 4);
  std::memcpy(vendor + 8, &c, 4);
  dump.vendor = vendor;
  detail::cpuid_leaf(1, &a, &b, &c, &d);
  dump.vmx_support = (c & (1U << 5)) != 0;
  dump.hypervisor_present = (c & (1U << 31)) != 0;
  if (dump.hypervisor_present) {
    detail::cpuid_leaf(0x40000000, &a, &b, &c, &d);
    char hv_vendor[13] = {};
    std::memcpy(hv_vendor, &b, 4);
    std::memcpy(hv_vendor + 4, &c, 4);
    std::memcpy(hv_vendor + 8, &d, 4);
    dump.hv_vendor = hv_vendor;
  }
  return dump;
}

std::string CpuidDump::describe() const {
  char buf[512];
  std::snprintf(buf, sizeof(buf),
                "CPU: vendor=%s max_leaf=%u\nVMX supported: %s\nHypervisor present: %s (vendor=%s)\n",
                vendor.c_str(), max_leaf, vmx_support ? "YES" : "NO",
                hypervisor_present ? "YES" : "NO", hv_vendor.c_str());
  return buf;
}

Result<uint64_t> read_msr_usermode(uint32_t msr) {
  return detail::safe_rdmsr(msr);
}

}  // namespace real::vmx
#endif  // LR_ARCH_X64
