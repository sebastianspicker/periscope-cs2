// mode_cpu_vmx.cpp — Embedded x64 CPUID / VMX / SVM probes for T3.

#include "real/mode/mode_internal.hpp"

#include <cstdint>
#include <cstring>
#include <string>

#if LR_ARCH_X64 && LR_COMPILER_MSVC
#include <intrin.h>
#endif

namespace real::mode::detail {

#if LR_ARCH_X64
void cpuid_leaf(std::uint32_t leaf, std::uint32_t* a, std::uint32_t* b,
                std::uint32_t* c, std::uint32_t* d) {
#if LR_COMPILER_MSVC
  int info[4];
  __cpuid(info, static_cast<int>(leaf));
  *a = static_cast<std::uint32_t>(info[0]);
  *b = static_cast<std::uint32_t>(info[1]);
  *c = static_cast<std::uint32_t>(info[2]);
  *d = static_cast<std::uint32_t>(info[3]);
#else
  __asm__ volatile("cpuid"
                   : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                   : "a"(leaf));
#endif
}

bool cpu_has_vmx() {
  std::uint32_t a = 0, b = 0, c = 0, d = 0;
  cpuid_leaf(1, &a, &b, &c, &d);
  return (c & (1u << 5)) != 0;  // CPUID.1:ECX.VMX
}

bool cpu_has_svm() {
  std::uint32_t a = 0, b = 0, c = 0, d = 0;
  cpuid_leaf(0x80000001u, &a, &b, &c, &d);
  return (c & (1u << 2)) != 0;  // CPUID.80000001:ECX.SVM
}

bool cpu_hypervisor_guest(std::string& vendor_out) {
  std::uint32_t a = 0, b = 0, c = 0, d = 0;
  cpuid_leaf(1, &a, &b, &c, &d);
  if ((c & (1u << 31)) == 0) return false;  // hypervisor present bit
  cpuid_leaf(0x40000000u, &a, &b, &c, &d);
  char vend[13] = {};
  std::memcpy(vend + 0, &b, 4);
  std::memcpy(vend + 4, &c, 4);
  std::memcpy(vend + 8, &d, 4);
  vendor_out = vend;
  return true;
}
#endif  // LR_ARCH_X64

}  // namespace real::mode::detail
