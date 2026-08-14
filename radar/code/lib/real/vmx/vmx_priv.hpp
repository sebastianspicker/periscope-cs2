// vmx_priv.hpp — Internal shared helpers for the VT-x stack.
// Not part of the public API. Privilege probes, safe MSR/CR access, and
// SEH-guarded privileged instruction wrappers so usermode CI never takes #UD.

#pragma once

#include "real/vmx/vmx_intrin.hpp"

#if LR_ARCH_X64

#include <cstdint>

#if LR_COMPILER_MSVC
#  include <intrin.h>
#  include <windows.h>
#endif

namespace real::vmx::detail {

// ── SEH islands (MSVC): must not live in functions with C++ unwinding ──
#if LR_COMPILER_MSVC
// MASM helpers from lib/real/win/{tlb,vmcall}_msvc.asm
extern "C" int invept_asm(unsigned int type, const void* descriptor);
extern "C" int invvpid_asm(unsigned int type, const void* descriptor);
extern "C" uint64_t vmcall_asm(uint64_t input, uint64_t param1, uint64_t param2);

namespace seh {

inline int probe_rdmsr_ok() {
  __try {
    (void)__readmsr(0x480);
    return 1;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return 0;
  }
}

inline int rdmsr(uint32_t msr, uint64_t* out) {
  __try {
    *out = static_cast<uint64_t>(__readmsr(msr));
    return 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

inline int wrmsr(uint32_t msr, uint64_t value) {
  __try {
    __writemsr(msr, value);
    return 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

inline int read_cr0(uint64_t* out) {
  __try {
    *out = __readcr0();
    return 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

inline int read_cr3(uint64_t* out) {
  __try {
    *out = __readcr3();
    return 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

inline int read_cr4(uint64_t* out) {
  __try {
    *out = __readcr4();
    return 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

inline int write_cr4(uint64_t value) {
  __try {
    __writecr4(value);
    return 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

inline int vmx_on(unsigned long long* phys) {
  __try {
    return __vmx_on(phys);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

inline int vmx_off() {
  __try {
    __vmx_off();
    return 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

inline int vmclear(unsigned long long* phys) {
  __try {
    return __vmx_vmclear(phys);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

inline int vmptrld(unsigned long long* phys) {
  __try {
    return __vmx_vmptrld(phys);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

inline int vmread(size_t field, uint64_t* value) {
  __try {
    return __vmx_vmread(field, value);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

inline int vmwrite(size_t field, uint64_t value) {
  __try {
    return __vmx_vmwrite(field, value);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

inline int vmlaunch() {
  __try {
    return __vmx_vmlaunch();
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

inline int vmresume() {
  __try {
    return __vmx_vmresume();
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

inline int invept(unsigned int type, const void* desc) {
  __try {
    return invept_asm(type, desc);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

inline int invvpid(unsigned int type, const void* desc) {
  __try {
    return invvpid_asm(type, desc);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

inline int vmcall(uint64_t input, uint64_t p1, uint64_t p2, uint64_t* out) {
  __try {
    *out = vmcall_asm(input, p1, p2);
    return 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

}  // namespace seh
#endif  // LR_COMPILER_MSVC

/// Return true when the current code segment runs at CPL=0.
inline bool at_cpl0() {
#if LR_COMPILER_MSVC
  static int cached = -1;
  if (cached >= 0) return cached != 0;
  cached = seh::probe_rdmsr_ok();
  return cached != 0;
#else
  unsigned short cs = 0;
  __asm__ volatile("mov %%cs, %0" : "=r"(cs));
  return (cs & 3u) == 0;
#endif
}

inline bool can_access_vmx_msrs() { return at_cpl0(); }

inline void cpuid_raw(uint32_t leaf, uint32_t subleaf,
                      uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
#if LR_COMPILER_MSVC
  int info[4];
  __cpuidex(info, static_cast<int>(leaf), static_cast<int>(subleaf));
  *a = static_cast<uint32_t>(info[0]);
  *b = static_cast<uint32_t>(info[1]);
  *c = static_cast<uint32_t>(info[2]);
  *d = static_cast<uint32_t>(info[3]);
#else
  __asm__ volatile("cpuid"
                   : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                   : "a"(leaf), "c"(subleaf));
#endif
}

inline void cpuid_leaf(uint32_t leaf, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
  cpuid_raw(leaf, 0, a, b, c, d);
}

inline Result<uint64_t> safe_rdmsr(uint32_t msr) {
  if (!can_access_vmx_msrs()) {
    return Result<uint64_t>(0, "RDMSR requires CPL=0");
  }
#if LR_COMPILER_MSVC
  uint64_t v = 0;
  if (seh::rdmsr(msr, &v) != 0) return Result<uint64_t>(0, "RDMSR raised exception");
  return Result<uint64_t>(v);
#else
  uint32_t low = 0, high = 0;
  __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
  return Result<uint64_t>((static_cast<uint64_t>(high) << 32) | low);
#endif
}

inline Result<void> safe_wrmsr(uint32_t msr, uint64_t value) {
  if (!can_access_vmx_msrs()) {
    return Result<void>("WRMSR requires CPL=0");
  }
#if LR_COMPILER_MSVC
  if (seh::wrmsr(msr, value) != 0) return Result<void>("WRMSR raised exception");
  return Result<void>();
#else
  __asm__ volatile("wrmsr"
                   :
                   : "a"(static_cast<uint32_t>(value)),
                     "d"(static_cast<uint32_t>(value >> 32)),
                     "c"(msr));
  return Result<void>();
#endif
}

inline Result<uint64_t> safe_read_cr0() {
#if LR_COMPILER_MSVC
  uint64_t v = 0;
  if (seh::read_cr0(&v) != 0) return Result<uint64_t>(0, "CR0 read failed");
  return Result<uint64_t>(v);
#else
  uint64_t v = 0;
  __asm__ volatile("mov %%cr0, %0" : "=r"(v));
  return Result<uint64_t>(v);
#endif
}

inline Result<uint64_t> safe_read_cr3() {
  if (!at_cpl0()) return Result<uint64_t>(0, "CR3 read requires CPL=0");
#if LR_COMPILER_MSVC
  uint64_t v = 0;
  if (seh::read_cr3(&v) != 0) return Result<uint64_t>(0, "CR3 read failed");
  return Result<uint64_t>(v);
#else
  uint64_t v = 0;
  __asm__ volatile("mov %%cr3, %0" : "=r"(v));
  return Result<uint64_t>(v);
#endif
}

inline Result<uint64_t> safe_read_cr4() {
  if (!at_cpl0()) return Result<uint64_t>(0, "CR4 read requires CPL=0");
#if LR_COMPILER_MSVC
  uint64_t v = 0;
  if (seh::read_cr4(&v) != 0) return Result<uint64_t>(0, "CR4 read failed");
  return Result<uint64_t>(v);
#else
  uint64_t v = 0;
  __asm__ volatile("mov %%cr4, %0" : "=r"(v));
  return Result<uint64_t>(v);
#endif
}

inline Result<void> safe_write_cr4(uint64_t value) {
  if (!at_cpl0()) return Result<void>("CR4 write requires CPL=0");
#if LR_COMPILER_MSVC
  if (seh::write_cr4(value) != 0) return Result<void>("CR4 write failed");
  return Result<void>();
#else
  __asm__ volatile("mov %0, %%cr4" : : "r"(value) : "memory");
  return Result<void>();
#endif
}

constexpr uint32_t kIa32FeatureControl     = 0x0000003A;
constexpr uint32_t kIa32VmxBasic           = 0x00000480;
constexpr uint32_t kIa32VmxPinbasedCtls    = 0x00000481;
constexpr uint32_t kIa32VmxProcbasedCtls   = 0x00000482;
constexpr uint32_t kIa32VmxExitCtls        = 0x00000483;
constexpr uint32_t kIa32VmxEntryCtls       = 0x00000484;
constexpr uint32_t kIa32VmxMisc            = 0x00000485;
constexpr uint32_t kIa32VmxCr0Fixed0       = 0x00000486;
constexpr uint32_t kIa32VmxCr0Fixed1       = 0x00000487;
constexpr uint32_t kIa32VmxCr4Fixed0       = 0x00000488;
constexpr uint32_t kIa32VmxCr4Fixed1       = 0x00000489;
constexpr uint32_t kIa32VmxProcbasedCtls2  = 0x0000048B;
constexpr uint32_t kIa32VmxEptVpidCap      = 0x0000048C;
constexpr uint32_t kIa32VmxTruePinbased    = 0x0000048D;
constexpr uint32_t kIa32VmxTrueProcbased   = 0x0000048E;
constexpr uint32_t kIa32VmxTrueExit        = 0x0000048F;
constexpr uint32_t kIa32VmxTrueEntry       = 0x00000490;

constexpr uint64_t kCr4Vmxe                = 1ULL << 13;
constexpr uint64_t kFeatureControlLock     = 1ULL << 0;
constexpr uint64_t kFeatureControlVmxOn    = 1ULL << 2;

inline uint32_t vmx_adjust_controls(uint32_t requested, uint64_t msr_value) {
  uint32_t allowed0 = static_cast<uint32_t>(msr_value);
  uint32_t allowed1 = static_cast<uint32_t>(msr_value >> 32);
  uint32_t result = requested;
  result |= allowed0;
  result &= allowed1;
  return result;
}

}  // namespace real::vmx::detail

#endif  // LR_ARCH_X64
