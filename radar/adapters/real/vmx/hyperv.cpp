// Hyper-V hypercalls and VMX capability probing.
// CPUID/HV leaf surfaces are fully unprivileged. VMCALL and capability MSRs
// use real instruction paths with pre-checks so usermode CI never crashes.
#include "real/vmx/vmx_intrin.hpp"
#include "real/vmx/vmx_priv.hpp"

#if LR_ARCH_X64

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if LR_COMPILER_MSVC
#  include <intrin.h>
#  include <malloc.h>
#endif

namespace real::vmx {

Result<bool> hyperv_hypercalls_available() {
  uint32_t a = 0, b = 0, c = 0, d = 0;
  detail::cpuid_leaf(1, &a, &b, &c, &d);
  if ((c & (1U << 31)) == 0) return Result<bool>(false);

  detail::cpuid_leaf(0x40000000, &a, &b, &c, &d);
  char vendor[13] = {};
  std::memcpy(vendor, &b, 4);
  std::memcpy(vendor + 4, &c, 4);
  std::memcpy(vendor + 8, &d, 4);
  if (std::strcmp(vendor, "Microsoft Hv") != 0 || a < 0x40000001) {
    return Result<bool>(false);
  }
  detail::cpuid_leaf(0x40000001, &a, &b, &c, &d);
  return Result<bool>((a & 1) != 0 || a != 0);
}

std::string platform_hv_vendor() {
  uint32_t a = 0, b = 0, c = 0, d = 0;
  detail::cpuid_leaf(1, &a, &b, &c, &d);
  if ((c & (1U << 31)) == 0) return {};
  detail::cpuid_leaf(0x40000000, &a, &b, &c, &d);
  char vendor[13] = {};
  std::memcpy(vendor, &b, 4);
  std::memcpy(vendor + 4, &c, 4);
  std::memcpy(vendor + 8, &d, 4);
  return std::string(vendor);
}

std::string hyperv_spoofed_vendor() {
  auto plat = platform_hv_vendor();
  if (!plat.empty()) return plat;
  return "Microsoft Hv";
}

void hyperv_spoofed_leaf_40000001(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) {
  auto plat = platform_hv_vendor();
  if (!plat.empty()) {
    detail::cpuid_leaf(0x40000001, &a, &b, &c, &d);
    return;
  }
  a = 0x1;
  b = 0x000A0002;
  c = 0x00000FFF;
  d = 0x00003E34;
}

void hyperv_spoofed_leaf(uint32_t leaf, uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) {
  if (leaf == 0x40000001) {
    hyperv_spoofed_leaf_40000001(a, b, c, d);
    return;
  }
  auto plat = platform_hv_vendor();
  if (!plat.empty() && leaf >= 0x40000000 && leaf <= 0x400000FF) {
    detail::cpuid_leaf(leaf, &a, &b, &c, &d);
    return;
  }
  a = b = c = d = 0;
}

Result<uint64_t> hyperv_hypercall(uint64_t input, uint64_t param1, uint64_t param2) {
  auto avail = hyperv_hypercalls_available();
  if (!avail) return Result<uint64_t>(0, avail.error_msg);
  if (!*avail) {
    return Result<uint64_t>(0, "Hyper-V hypercalls not available on this host");
  }

#if LR_COMPILER_MSVC
  uint64_t result = 0;
  if (detail::seh::vmcall(input, param1, param2, &result) != 0) {
    return Result<uint64_t>(0, "VMCALL raised exception");
  }
  return Result<uint64_t>(result);
#else
  register uint64_t r8_param __asm__("r8") = param2;
  uint64_t result = 0;
  __asm__ volatile("vmcall"
                   : "=a"(result)
                   : "c"(input), "d"(param1), "r"(r8_param)
                   : "memory", "cc");
  return Result<uint64_t>(result);
#endif
}

Result<std::vector<uint8_t>> hyperv_read_virtual_memory(uint64_t guest_phys_addr, size_t size) {
  if (size == 0) return Result<std::vector<uint8_t>>(std::vector<uint8_t>{});

  auto avail = hyperv_hypercalls_available();
  if (!avail || !*avail) {
    return Result<std::vector<uint8_t>>({}, "Hyper-V hypercalls not available");
  }

  constexpr uint64_t kHvCallReadVirtualMemory = 0x00F0;
  constexpr uint64_t kHvFastHypercall = (1ULL << 16);

  void* out_buf = nullptr;
#if LR_COMPILER_MSVC
  out_buf = _aligned_malloc(4096, 4096);
#else
  if (posix_memalign(&out_buf, 4096, 4096) != 0) out_buf = nullptr;
#endif
  if (!out_buf) return Result<std::vector<uint8_t>>({}, "Output buffer allocation failed");
  std::memset(out_buf, 0, 4096);

  auto free_buf = [&]() {
#if LR_COMPILER_MSVC
    _aligned_free(out_buf);
#else
    std::free(out_buf);
#endif
  };

  auto buf_phys = virt_to_phys(out_buf);
  if (!buf_phys) {
    free_buf();
    return Result<std::vector<uint8_t>>(
        {}, std::string("Cannot get output buffer physical address: ") + buf_phys.error_msg);
  }

  std::vector<uint8_t> result(size);
  size_t total_read = 0;

  for (size_t offset = 0; offset < size; offset += 4096) {
    size_t remaining = size - offset;
    size_t chunk = remaining < static_cast<size_t>(4096) ? remaining : static_cast<size_t>(4096);
    uint64_t target_pa = guest_phys_addr + offset;
    uint64_t input = kHvCallReadVirtualMemory | kHvFastHypercall;
    uint64_t param1 = target_pa;
    uint64_t param2 = (*buf_phys) | (static_cast<uint64_t>(chunk) << 12);

    auto status = hyperv_hypercall(input, param1, param2);
    if (!status) {
      free_buf();
      return Result<std::vector<uint8_t>>(
          {}, std::string("HvCallReadVirtualMemory failed: ") + status.error_msg);
    }
    std::memcpy(result.data() + total_read, out_buf, chunk);
    total_read += chunk;
  }

  free_buf();
  return Result<std::vector<uint8_t>>(std::move(result));
}

Result<VmxCapabilities> get_vmx_capabilities() {
  VmxCapabilities caps;
  auto supported = vmx_supported();
  if (!supported) return Result<VmxCapabilities>(caps, supported.error_msg);
  caps.vmx_enabled = *supported;

  if (!detail::can_access_vmx_msrs()) {
    caps.msr_probe_ok = false;
    return Result<VmxCapabilities>(caps);
  }

  auto ept_vpid = detail::safe_rdmsr(detail::kIa32VmxEptVpidCap);
  auto proc2 = detail::safe_rdmsr(detail::kIa32VmxProcbasedCtls2);
  if (!ept_vpid) {
    caps.msr_probe_ok = false;
    return Result<VmxCapabilities>(caps, ept_vpid.error_msg);
  }
  caps.msr_probe_ok = true;
  const uint64_t ept_vpid_val = *ept_vpid;
  caps.ept_supported = (ept_vpid_val & 1) != 0;
  caps.vpid_supported = (ept_vpid_val & 2) != 0;
  caps.invept_supported = (ept_vpid_val & (1ULL << 20)) != 0;
  caps.invvpid_supported =
      (ept_vpid_val & (1ULL << 32)) != 0 || (ept_vpid_val & (1ULL << 40)) != 0;
  caps.max_ept_levels = caps.ept_supported ? 4 : 0;
  if (proc2) {
    caps.unrestricted_guest = ((*proc2 >> 32) & (1ULL << 7)) != 0;
  }
  return Result<VmxCapabilities>(caps);
}

std::string VmxCapabilities::describe() const {
  char buf[640];
  std::snprintf(buf, sizeof(buf),
                "VMX Capabilities:\n"
                "  VMX enabled: %s\n"
                "  EPT: %s\n"
                "  VPID: %s\n"
                "  Unrestricted guest: %s\n"
                "  INVEPT: %s\n"
                "  INVVPID: %s\n"
                "  Max EPT levels: %d\n"
                "  MSR probe: %s\n",
                vmx_enabled ? "YES" : "NO", ept_supported ? "YES" : "NO",
                vpid_supported ? "YES" : "NO", unrestricted_guest ? "YES" : "NO",
                invept_supported ? "YES" : "NO", invvpid_supported ? "YES" : "NO",
                max_ept_levels, msr_probe_ok ? "OK" : "unavailable (usermode)");
  return buf;
}

}  // namespace real::vmx
#endif  // LR_ARCH_X64
