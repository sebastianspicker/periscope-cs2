// VMX TLB invalidation: INVEPT / INVVPID with privilege pre-checks.
#include "real/vmx/vmx_intrin.hpp"
#include "real/vmx/vmx_priv.hpp"

#if LR_ARCH_X64

namespace real::vmx {

Result<void> invept(int type, uint64_t eptp) {
  if (!detail::at_cpl0()) {
    return Result<void>("INVEPT requires VMX root (CPL=0)");
  }
  struct Descriptor {
    uint64_t eptp;
    uint64_t reserved;
  } descriptor{eptp, 0};

#if LR_COMPILER_MSVC
  if (detail::seh::invept(static_cast<unsigned int>(type), &descriptor) != 0) {
    return Result<void>("INVEPT failed");
  }
  return Result<void>();
#else
  // AT&T: invept m128, r64  ≡  Intel INVEPT r64, m128
  unsigned char failed = 1;
  __asm__ volatile("invept %2, %1; setna %0"
                   : "=qm"(failed)
                   : "r"(static_cast<uint64_t>(type)), "m"(descriptor)
                   : "memory", "cc");
  return failed ? Result<void>("INVEPT failed") : Result<void>();
#endif
}

Result<void> invvpid(int type, uint16_t vpid, uint64_t linear_addr) {
  if (!detail::at_cpl0()) {
    return Result<void>("INVVPID requires VMX root (CPL=0)");
  }
  struct Descriptor {
    uint64_t vpid;
    uint64_t linear_address;
  } descriptor{vpid, linear_addr};

#if LR_COMPILER_MSVC
  if (detail::seh::invvpid(static_cast<unsigned int>(type), &descriptor) != 0) {
    return Result<void>("INVVPID failed");
  }
  return Result<void>();
#else
  unsigned char failed = 1;
  __asm__ volatile("invvpid %2, %1; setna %0"
                   : "=qm"(failed)
                   : "r"(static_cast<uint64_t>(type)), "m"(descriptor)
                   : "memory", "cc");
  return failed ? Result<void>("INVVPID failed") : Result<void>();
#endif
}

}  // namespace real::vmx
#endif  // LR_ARCH_X64
