// VMX lifecycle: enable, VMXON/VMXOFF, virt→phys, region allocation.
// Privileged paths are real instruction sequences gated by CPL=0 / SEH.
#include "real/vmx/vmx_intrin.hpp"
#include "real/vmx/vmx_priv.hpp"

#if LR_ARCH_X64

#include <cstdlib>
#include <cstring>

#if LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <unistd.h>
#endif

#if LR_COMPILER_MSVC
#  include <intrin.h>
#  include <malloc.h>
#endif

namespace real::vmx {

Result<bool> is_hypervisor_guest() {
  uint32_t a = 0, b = 0, c = 0, d = 0;
  detail::cpuid_leaf(1, &a, &b, &c, &d);
  return Result<bool>((c & (1U << 31)) != 0);
}

Result<void> enable_vmx() {
  auto supported = vmx_supported();
  if (!supported) return Result<void>(supported.error_msg);
  if (!*supported) return Result<void>("CPU does not support VMX");

  if (!detail::at_cpl0()) {
    return Result<void>("VMX enable requires CPL=0 (CR4.VMXE / IA32_FEATURE_CONTROL)");
  }

  auto fc = detail::safe_rdmsr(detail::kIa32FeatureControl);
  if (!fc) return Result<void>(fc.error_msg);
  uint64_t feature_control = *fc;

  if ((feature_control & detail::kFeatureControlLock) != 0) {
    if ((feature_control & detail::kFeatureControlVmxOn) == 0) {
      return Result<void>("IA32_FEATURE_CONTROL locked with VMX outside SMX disabled");
    }
  } else {
    feature_control |= detail::kFeatureControlVmxOn | detail::kFeatureControlLock;
    auto wr = detail::safe_wrmsr(detail::kIa32FeatureControl, feature_control);
    if (!wr) return wr;
  }

  auto cr4 = detail::safe_read_cr4();
  if (!cr4) return Result<void>(cr4.error_msg);
  if ((*cr4 & detail::kCr4Vmxe) == 0) {
    auto wr = detail::safe_write_cr4(*cr4 | detail::kCr4Vmxe);
    if (!wr) return wr;
  }
  return Result<void>();
}

Result<void> enable_vmx_from_usermode() {
  return enable_vmx();
}

Result<void*> vmxon_alloc() {
  void* region = nullptr;
#if LR_COMPILER_MSVC
  region = _aligned_malloc(4096, 4096);
  if (region == nullptr) return Result<void*>(nullptr, "VMXON allocation failed");
#else
  if (posix_memalign(&region, 4096, 4096) != 0 || region == nullptr) {
    return Result<void*>(nullptr, "VMXON allocation failed");
  }
#endif
  std::memset(region, 0, 4096);

  uint32_t revision = 0;
  auto rev = detail::safe_rdmsr(detail::kIa32VmxBasic);
  if (rev) {
    revision = static_cast<uint32_t>(*rev & 0x7fffffffU);
  }
  std::memcpy(region, &revision, sizeof(revision));
  return Result<void*>(region);
}

void vmx_region_free(void* region) {
  if (region == nullptr) return;
#if LR_COMPILER_MSVC
  _aligned_free(region);
#else
  std::free(region);
#endif
}

Result<void> vmxon(void* region) {
  if (region == nullptr) return Result<void>("VMXON region is null");
  if (!detail::at_cpl0()) {
    return Result<void>("VMXON requires CPL=0");
  }

  auto phys_result = virt_to_phys(region);
  if (!phys_result) {
    return Result<void>(std::string("VMXON requires physical address: ") + phys_result.error_msg);
  }
  uint64_t phys_addr = *phys_result;

#if LR_COMPILER_MSVC
  unsigned long long phys = phys_addr;
  int rc = detail::seh::vmx_on(&phys);
  if (rc != 0) return Result<void>("VMXON failed (instruction returned failure or exception)");
#else
  unsigned char failed = 1;
  __asm__ volatile("vmxon %1; setna %0"
                   : "=qm"(failed)
                   : "m"(phys_addr)
                   : "memory", "cc");
  if (failed) return Result<void>("VMXON failed");
#endif
  return Result<void>();
}

Result<void> vmxoff() {
  if (!detail::at_cpl0()) {
    return Result<void>("VMXOFF requires CPL=0");
  }
#if LR_COMPILER_MSVC
  if (detail::seh::vmx_off() != 0) return Result<void>("VMXOFF raised exception");
#else
  __asm__ volatile("vmxoff" ::: "memory", "cc");
#endif
  auto cr4 = detail::safe_read_cr4();
  if (cr4 && (*cr4 & detail::kCr4Vmxe) != 0) {
    (void)detail::safe_write_cr4(*cr4 & ~detail::kCr4Vmxe);
  }
  return Result<void>();
}

Result<uint64_t> virt_to_phys(void* virt_addr) {
  if (virt_addr == nullptr) {
    return Result<uint64_t>(0, "virt_to_phys: null address");
  }
#if LR_PLATFORM_LINUX
  int fd = open("/proc/self/pagemap", O_RDONLY);
  if (fd < 0) return Result<uint64_t>(0, "Cannot open /proc/self/pagemap");
  uint64_t pfn_entry = 0;
  const off_t offset =
      static_cast<off_t>((reinterpret_cast<uintptr_t>(virt_addr) / 4096) * sizeof(uint64_t));
  if (pread(fd, &pfn_entry, sizeof(pfn_entry), offset) != static_cast<ssize_t>(sizeof(pfn_entry))) {
    close(fd);
    return Result<uint64_t>(0, "pagemap read failed");
  }
  close(fd);
  if ((pfn_entry & (1ULL << 63)) == 0) {
    return Result<uint64_t>(0, "Page not present in pagemap");
  }
  uint64_t phys = ((pfn_entry & 0x007FFFFFFFFFFFFFULL) * 4096) +
                  (reinterpret_cast<uintptr_t>(virt_addr) & 0xFFFULL);
  return Result<uint64_t>(phys);
#else
  (void)virt_addr;
  return Result<uint64_t>(0, "virt_to_phys requires Linux pagemap or kernel driver on this platform");
#endif
}

}  // namespace real::vmx
#endif  // LR_ARCH_X64
