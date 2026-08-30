// VMCS manipulation, pure field composition, and VM-entry instructions.
#include "real/vmx/vmx_intrin.hpp"
#include "real/vmx/vmx_priv.hpp"

#if LR_ARCH_X64

#include <cstdlib>
#include <cstring>
#include <vector>

#if LR_COMPILER_MSVC
#  include <intrin.h>
#  include <malloc.h>
#endif

namespace real::vmx {
namespace {

bool vmread_impl(VmcsField field, uint64_t* value) {
#if LR_COMPILER_MSVC
  return detail::seh::vmread(static_cast<size_t>(field), value) == 0;
#else
  unsigned char failed = 1;
  __asm__ volatile("vmread %2, %1; setna %0"
                   : "=qm"(failed), "=rm"(*value)
                   : "r"(static_cast<uint64_t>(field))
                   : "cc");
  return failed == 0;
#endif
}

bool vmwrite_impl(VmcsField field, uint64_t value) {
#if LR_COMPILER_MSVC
  return detail::seh::vmwrite(static_cast<size_t>(field), value) == 0;
#else
  unsigned char failed = 1;
  __asm__ volatile("vmwrite %1, %2; setna %0"
                   : "=qm"(failed)
                   : "r"(static_cast<uint64_t>(field)), "rm"(value)
                   : "cc");
  return failed == 0;
#endif
}

uint32_t revision_id() {
  auto rev = detail::safe_rdmsr(detail::kIa32VmxBasic);
  if (!rev) return 0;
  return static_cast<uint32_t>(*rev & 0x7fffffffU);
}

void adjust_controls_from_msrs(VmcsSetupConfig& cfg) {
  if (!detail::can_access_vmx_msrs()) return;

  auto basic = detail::safe_rdmsr(detail::kIa32VmxBasic);
  const bool use_true = basic && ((*basic & (1ULL << 55)) != 0);

  auto pin_msr = detail::safe_rdmsr(use_true ? detail::kIa32VmxTruePinbased
                                             : detail::kIa32VmxPinbasedCtls);
  auto proc_msr = detail::safe_rdmsr(use_true ? detail::kIa32VmxTrueProcbased
                                              : detail::kIa32VmxProcbasedCtls);
  auto exit_msr = detail::safe_rdmsr(use_true ? detail::kIa32VmxTrueExit
                                              : detail::kIa32VmxExitCtls);
  auto entry_msr = detail::safe_rdmsr(use_true ? detail::kIa32VmxTrueEntry
                                               : detail::kIa32VmxEntryCtls);
  auto sec_msr = detail::safe_rdmsr(detail::kIa32VmxProcbasedCtls2);

  if (pin_msr) cfg.pin_based = detail::vmx_adjust_controls(cfg.pin_based, *pin_msr);
  if (proc_msr) cfg.cpu_based = detail::vmx_adjust_controls(cfg.cpu_based, *proc_msr);
  if (exit_msr) cfg.exit_controls = detail::vmx_adjust_controls(cfg.exit_controls, *exit_msr);
  if (entry_msr) cfg.entry_controls = detail::vmx_adjust_controls(cfg.entry_controls, *entry_msr);
  if (sec_msr) cfg.secondary = detail::vmx_adjust_controls(cfg.secondary, *sec_msr);
}

}  // namespace

std::vector<VmcsFieldWrite> compose_basic_vmcs_fields(const VmcsSetupConfig& cfg) {
  std::vector<VmcsFieldWrite> out;
  out.reserve(48);

  auto push = [&](VmcsField f, uint64_t v) { out.push_back(VmcsFieldWrite{f, v}); };

  push(VmcsField::PinBasedExecControls, cfg.pin_based);
  push(VmcsField::CpuBasedExecControls, cfg.cpu_based);
  push(VmcsField::SecondaryExecControls, cfg.secondary);
  push(VmcsField::ExceptionBitmap, 0);
  push(VmcsField::PageFaultErrorCodeMask, 0);
  push(VmcsField::PageFaultErrorCodeMatch, 0);
  push(VmcsField::Cr3TargetCount, 0);
  push(VmcsField::TprThreshold, 0);

  push(VmcsField::VmExitControls, cfg.exit_controls);
  push(VmcsField::VmExitMsrStoreCount, 0);
  push(VmcsField::VmExitMsrLoadCount, 0);
  push(VmcsField::VmEntryControls, cfg.entry_controls);
  push(VmcsField::VmEntryMsrLoadCount, 0);
  push(VmcsField::VmEntryIntrInfo, 0);

  if (cfg.eptp != 0) {
    push(VmcsField::Eptp, cfg.eptp);
  }
  if (cfg.vpid != 0) {
    push(VmcsField::Vpid, cfg.vpid);
  }

  push(VmcsField::GuestCr0, cfg.guest_cr0);
  push(VmcsField::GuestCr3, cfg.guest_cr3);
  push(VmcsField::GuestCr4, cfg.guest_cr4);
  push(VmcsField::GuestDr7, 0x400);
  push(VmcsField::GuestRsp, cfg.guest_rsp);
  push(VmcsField::GuestRip, cfg.guest_rip);
  push(VmcsField::GuestRflags, cfg.guest_rflags);
  push(VmcsField::GuestIa32Efer, cfg.guest_efer);
  push(VmcsField::VmcsLinkPointer, 0xFFFFFFFFFFFFFFFFULL);
  push(VmcsField::GuestActivityState, 0);
  push(VmcsField::GuestInterruptibility, 0);
  push(VmcsField::GuestPendingDbgExcept, 0);

  push(VmcsField::GuestCsSelector, 0x10);
  push(VmcsField::GuestSsSelector, 0x18);
  push(VmcsField::GuestDsSelector, 0x18);
  push(VmcsField::GuestEsSelector, 0x18);
  push(VmcsField::GuestFsSelector, 0);
  push(VmcsField::GuestGsSelector, 0);
  push(VmcsField::GuestTrSelector, 0x18);
  push(VmcsField::GuestLdtrSelector, 0);

  push(VmcsField::GuestCsBase, 0);
  push(VmcsField::GuestSsBase, 0);
  push(VmcsField::GuestDsBase, 0);
  push(VmcsField::GuestEsBase, 0);
  push(VmcsField::GuestFsBase, 0);
  push(VmcsField::GuestGsBase, 0);
  push(VmcsField::GuestTrBase, 0);
  push(VmcsField::GuestLdtrBase, 0);
  push(VmcsField::GuestGdtrBase, 0);
  push(VmcsField::GuestIdtrBase, 0);

  push(VmcsField::GuestCsLimit, 0xFFFFFFFF);
  push(VmcsField::GuestSsLimit, 0xFFFFFFFF);
  push(VmcsField::GuestDsLimit, 0xFFFFFFFF);
  push(VmcsField::GuestEsLimit, 0xFFFFFFFF);
  push(VmcsField::GuestFsLimit, 0xFFFFFFFF);
  push(VmcsField::GuestGsLimit, 0xFFFFFFFF);
  push(VmcsField::GuestTrLimit, 0x67);
  push(VmcsField::GuestLdtrLimit, 0);
  push(VmcsField::GuestGdtrLimit, 0xFFFF);
  push(VmcsField::GuestIdtrLimit, 0xFFFF);

  push(VmcsField::GuestCsAccessRights, 0xA09B);
  push(VmcsField::GuestSsAccessRights, 0xC093);
  push(VmcsField::GuestDsAccessRights, 0xC093);
  push(VmcsField::GuestEsAccessRights, 0xC093);
  push(VmcsField::GuestFsAccessRights, 0xC093);
  push(VmcsField::GuestGsAccessRights, 0xC093);
  push(VmcsField::GuestTrAccessRights, 0x8B);
  push(VmcsField::GuestLdtrAccessRights, 0x10000);

  push(VmcsField::HostCr0, cfg.host_cr0);
  push(VmcsField::HostCr3, cfg.host_cr3);
  push(VmcsField::HostCr4, cfg.host_cr4);
  push(VmcsField::HostRsp, cfg.host_rsp);
  push(VmcsField::HostRip, cfg.host_rip);
  push(VmcsField::HostIa32Efer, cfg.host_efer);

  push(VmcsField::HostCsSelector, 0x10);
  push(VmcsField::HostSsSelector, 0x18);
  push(VmcsField::HostDsSelector, 0x18);
  push(VmcsField::HostEsSelector, 0x18);
  push(VmcsField::HostFsSelector, 0);
  push(VmcsField::HostGsSelector, 0);
  push(VmcsField::HostTrSelector, 0x18);

  push(VmcsField::Cr0GuestHostMask, 0);
  push(VmcsField::Cr4GuestHostMask, 0);
  push(VmcsField::Cr0ReadShadow, cfg.guest_cr0);
  push(VmcsField::Cr4ReadShadow, cfg.guest_cr4);

  return out;
}

Result<void*> vmcs_alloc() {
  void* vmcs = nullptr;
#if LR_COMPILER_MSVC
  vmcs = _aligned_malloc(4096, 4096);
  if (vmcs == nullptr) return Result<void*>(nullptr, "VMCS allocation failed");
#else
  if (posix_memalign(&vmcs, 4096, 4096) != 0 || vmcs == nullptr) {
    return Result<void*>(nullptr, "VMCS allocation failed");
  }
#endif
  std::memset(vmcs, 0, 4096);
  const uint32_t revision = revision_id();
  std::memcpy(vmcs, &revision, sizeof(revision));
  return Result<void*>(vmcs);
}

Result<void> vmclear(void* vmcs) {
  if (vmcs == nullptr) return Result<void>("VMCLEAR: null VMCS");
  if (!detail::at_cpl0()) return Result<void>("VMCLEAR requires CPL=0 / VMX root");

  auto phys_result = virt_to_phys(vmcs);
  if (!phys_result) return Result<void>(std::string("VMCLEAR: ") + phys_result.error_msg);
  uint64_t phys_addr = *phys_result;
#if LR_COMPILER_MSVC
  unsigned long long phys = phys_addr;
  if (detail::seh::vmclear(&phys) != 0) return Result<void>("VMCLEAR failed");
#else
  unsigned char failed = 1;
  __asm__ volatile("vmclear %1; setna %0" : "=qm"(failed) : "m"(phys_addr) : "memory", "cc");
  if (failed) return Result<void>("VMCLEAR failed");
#endif
  return Result<void>();
}

Result<void> vmptrld(void* vmcs) {
  if (vmcs == nullptr) return Result<void>("VMPTRLD: null VMCS");
  if (!detail::at_cpl0()) return Result<void>("VMPTRLD requires CPL=0 / VMX root");

  auto phys_result = virt_to_phys(vmcs);
  if (!phys_result) return Result<void>(std::string("VMPTRLD: ") + phys_result.error_msg);
  uint64_t phys_addr = *phys_result;
#if LR_COMPILER_MSVC
  unsigned long long phys = phys_addr;
  if (detail::seh::vmptrld(&phys) != 0) return Result<void>("VMPTRLD failed");
#else
  unsigned char failed = 1;
  __asm__ volatile("vmptrld %1; setna %0" : "=qm"(failed) : "m"(phys_addr) : "memory", "cc");
  if (failed) return Result<void>("VMPTRLD failed");
#endif
  return Result<void>();
}

Result<uint64_t> vmread(VmcsField field) {
  if (!detail::at_cpl0()) return Result<uint64_t>(0, "VMREAD requires VMX root");
  uint64_t value = 0;
  if (!vmread_impl(field, &value)) return Result<uint64_t>(0, "VMREAD failed");
  return Result<uint64_t>(value);
}

Result<void> vmwrite(VmcsField field, uint64_t value) {
  if (!detail::at_cpl0()) return Result<void>("VMWRITE requires VMX root");
  if (!vmwrite_impl(field, value)) return Result<void>("VMWRITE failed");
  return Result<void>();
}

Result<VmxStatus> vmlaunch() {
  if (!detail::at_cpl0()) {
    return Result<VmxStatus>(VmxStatus::FailInvalid, "VMLAUNCH requires VMX root");
  }
#if LR_COMPILER_MSVC
  if (detail::seh::vmlaunch() != 0) {
    return Result<VmxStatus>(VmxStatus::FailInvalid);
  }
#else
  unsigned char failed = 1;
  __asm__ volatile("vmlaunch; setna %0" : "=qm"(failed) :: "memory", "cc");
  if (!failed) return Result<VmxStatus>(VmxStatus::Success);
#endif
  return Result<VmxStatus>(VmxStatus::FailWithStatus);
}

Result<VmxStatus> vmresume() {
  if (!detail::at_cpl0()) {
    return Result<VmxStatus>(VmxStatus::FailInvalid, "VMRESUME requires VMX root");
  }
#if LR_COMPILER_MSVC
  if (detail::seh::vmresume() != 0) {
    return Result<VmxStatus>(VmxStatus::FailInvalid);
  }
#else
  unsigned char failed = 1;
  __asm__ volatile("vmresume; setna %0" : "=qm"(failed) :: "memory", "cc");
  if (!failed) return Result<VmxStatus>(VmxStatus::Success);
#endif
  return Result<VmxStatus>(VmxStatus::FailWithStatus);
}

Result<void*> setup_vmcs_ex(void* vmxon_region, const VmcsSetupConfig& cfg_in) {
  (void)vmxon_region;
  VmcsSetupConfig cfg = cfg_in;
  adjust_controls_from_msrs(cfg);

  if (cfg.host_cr0 == 0) {
    auto r = detail::safe_read_cr0();
    if (r) cfg.host_cr0 = *r;
  }
  if (cfg.host_cr3 == 0) {
    auto r = detail::safe_read_cr3();
    if (r) cfg.host_cr3 = *r;
  }
  if (cfg.host_cr4 == 0) {
    auto r = detail::safe_read_cr4();
    if (r) cfg.host_cr4 = *r;
  }

  auto fields = compose_basic_vmcs_fields(cfg);
  if (fields.empty()) {
    return Result<void*>(nullptr, "compose_basic_vmcs_fields produced empty table");
  }

  auto vmcs = vmcs_alloc();
  if (!vmcs) return Result<void*>(nullptr, vmcs.error_msg);

  if (!detail::at_cpl0()) {
    vmx_region_free(*vmcs);
    return Result<void*>(nullptr,
                         "setup_vmcs: VMCLEAR/VMPTRLD/VMWRITE require CPL=0 (field table is pure)");
  }

  auto cleared = vmclear(*vmcs);
  if (!cleared) {
    vmx_region_free(*vmcs);
    return Result<void*>(nullptr, cleared.error_msg);
  }
  auto loaded = vmptrld(*vmcs);
  if (!loaded) {
    vmx_region_free(*vmcs);
    return Result<void*>(nullptr, loaded.error_msg);
  }

  for (const auto& w : fields) {
    auto r = vmwrite(w.field, w.value);
    if (!r) {
      vmx_region_free(*vmcs);
      return Result<void*>(nullptr, r.error_msg);
    }
  }
  return Result<void*>(*vmcs);
}

Result<void*> setup_vmcs(void* vmxon_region, uint64_t guest_rip, uint64_t guest_rsp,
                         uint64_t guest_cr3) {
  VmcsSetupConfig cfg;
  cfg.guest_rip = guest_rip;
  cfg.guest_rsp = guest_rsp;
  cfg.guest_cr3 = guest_cr3;
  return setup_vmcs_ex(vmxon_region, cfg);
}

}  // namespace real::vmx
#endif  // LR_ARCH_X64
