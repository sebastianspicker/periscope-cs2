// smram.cpp — SMRAM / TSEG discovery and lock-state probing.
//
// TECHNIQUE: Intel host-bridge TSEGMB (PCI 0:0.0 + 0xB8), SMRR MSRs,
// and FADT-derived platform hints. SMRAM holds SMI handlers (TSEG).
//
// SCAR: Reading PCI config / MSR is normal for diagnostics; dumping
// SMRAM contents is not (and usually blocked once D_LCK is set).
//
// BLUE: Assert TSEG locked early in boot; verify SMM_Code_Chk_En;
// remote attestation of firmware measurements covering SMM.
//
// MITIGATION: BIOS "SMM lockdown", STM, Boot Guard, BIOS Guard.

#include "real/smm/smm_interface.hpp"
#include "real/memory.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <unistd.h>
#elif LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#endif

namespace real::smm {
namespace {

#if LR_PLATFORM_LINUX
// Read 32-bit PCI config via sysfs for domain 0000 bus 00 dev 00 fn 00.
Result<std::uint32_t> read_pci_config_u32(std::uint32_t offset) {
  char path[128];
  std::snprintf(path, sizeof(path),
                "/sys/bus/pci/devices/0000:00:00.0/config");
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    auto err = os_error("open host bridge PCI config");
    return Result<std::uint32_t>(0, err.error_msg);
  }
  std::uint32_t value = 0;
  if (pread(fd, &value, sizeof(value), offset) !=
      static_cast<ssize_t>(sizeof(value))) {
    close(fd);
    auto err = os_error("pread PCI config");
    return Result<std::uint32_t>(0, err.error_msg);
  }
  close(fd);
  return value;
}

Result<std::uint64_t> read_msr(std::uint32_t index) {
  int fd = open("/dev/cpu/0/msr", O_RDONLY);
  if (fd < 0) fd = open("/dev/msr0", O_RDONLY);
  if (fd < 0) {
    auto err = os_error("open MSR for SMRR");
    return Result<std::uint64_t>(0, err.error_msg);
  }
  std::uint64_t value = 0;
  if (pread(fd, &value, sizeof(value), index) !=
      static_cast<ssize_t>(sizeof(value))) {
    close(fd);
    auto err = os_error("pread MSR");
    return Result<std::uint64_t>(0, err.error_msg);
  }
  close(fd);
  return value;
}
#endif

#if LR_PLATFORM_WINDOWS
// SetupDi / cfgmgr free-form is heavy; use GetSystemFirmwareTable + optional
// lab driver for PCI config. For educational discovery, also try reading
// SMBIOS/ACPI-derived hints and report partial info.
#endif

}  // namespace

Result<SmramInfo> discover_smram() {
  SmramInfo info{};
  info.source = "unknown";

#if LR_PLATFORM_LINUX
  // 1) TSEGMB from Intel host bridge
  auto tsegmb = read_pci_config_u32(kIntelTsegmbOffset);
  if (tsegmb) {
    // Estimate size from BGSM (0xB4) - TSEGMB when both present.
    std::uint64_t size_hint = 0;
    auto bgsm = read_pci_config_u32(0xB4);
    auto decoded = decode_tsegmb(*tsegmb, 0);
    if (bgsm && decoded.valid) {
      const std::uint64_t bgsm_base =
          static_cast<std::uint64_t>((*bgsm) & kTsegmbBaseMask);
      if (bgsm_base > decoded.base) size_hint = bgsm_base - decoded.base;
    }
    decoded = decode_tsegmb(*tsegmb, size_hint);
    if (decoded.valid) {
      info.base = decoded.base;
      info.size = decoded.size;
      info.locked = decoded.locked;
      info.source = "tsegmb";
      info.detail = "Intel host-bridge TSEGMB";
    }
  }

  // 2) SMRR MSRs when available
  auto smrr_base = read_msr(kMsrSmrrPhysBase);
  auto smrr_mask = read_msr(kMsrSmrrPhysMask);
  if (smrr_base && smrr_mask) {
    const std::uint64_t base = (*smrr_base) & 0xFFFFF000ull;
    const std::uint64_t mask = (*smrr_mask) & 0xFFFFF000ull;
    const bool valid = ((*smrr_mask) & 0x800ull) != 0;  // VLD bit often bit 11
    if (valid && base) {
      info.smrr_active = true;
      if (info.base == 0) {
        info.base = base;
        // Size from mask: contiguous 1s in mask → region size
        if (mask) {
          info.size = (~mask + 1ull) & 0xFFFFF000ull;
        }
        info.source = "smrr";
        info.detail = "IA32_SMRR_PHYSBASE/MASK";
      }
    }
  }

  // 3) SMM feature control (best-effort)
  auto feat = read_msr(kMsrSmmFeatureControl);
  if (feat) {
    info.code_chk_en = ((*feat) & kSmmCodeChkEnBit) != 0;
  }

  if (info.base == 0) {
    // Still return partial success with detail for lab environments.
    info.detail = "SMRAM not discoverable (no TSEGMB/SMRR access)";
    return info;
  }
  std::printf("[smm] SMRAM base=0x%llx size=0x%llx locked=%d source=%s\n",
              static_cast<unsigned long long>(info.base),
              static_cast<unsigned long long>(info.size),
              info.locked ? 1 : 0, info.source.c_str());
  return info;

#elif LR_PLATFORM_WINDOWS
  // Windows: physical PCI config needs a driver. Expose FADT SMI info and
  // structured empty TSEG discovery so callers get a real Result path.
  auto fadt = read_fadt_smi_info();
  if (fadt && fadt->ok) {
    info.detail = "FADT SMI_CMD=0x" +
                  std::to_string(fadt->smi_cmd) +
                  "; TSEG requires kernel PCI config read on Windows";
    info.source = "fadt";
  } else {
    info.detail =
        "SMRAM discovery on Windows requires lab kernel driver for PCI config";
  }
  // Attempt WinRing0 READ_PCI_CONFIG if present (IOCTL 0x9C4020C0 common).
  HANDLE h = ::CreateFileA("\\\\.\\WinRing0_1_2_0", GENERIC_READ | GENERIC_WRITE,
                           0, nullptr, OPEN_EXISTING, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    h = ::CreateFileA("\\\\.\\WinRing0", GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                      OPEN_EXISTING, 0, nullptr);
  }
  if (h != INVALID_HANDLE_VALUE) {
    // OLS_READ_PCI_CONFIG: input bus/dev/fn/reg
    struct {
      ULONG pci_address;  // (bus<<8)|(dev<<3)|fn  encoding used by WinRing0
      ULONG reg_address;
      BYTE bytes;
    } pci_in{};
    pci_in.pci_address = 0;  // 0:0.0
    pci_in.reg_address = kIntelTsegmbOffset;
    pci_in.bytes = 4;
    std::uint32_t tsegmb = 0;
    DWORD ret = 0;
    const DWORD ioctl = 0x9C4020C0;  // IOCTL_OLS_READ_PCI_CONFIG
    if (::DeviceIoControl(h, ioctl, &pci_in, sizeof(pci_in), &tsegmb,
                          sizeof(tsegmb), &ret, nullptr)) {
      auto decoded = decode_tsegmb(tsegmb, 0);
      if (decoded.valid) {
        info.base = decoded.base;
        info.locked = decoded.locked;
        info.source = "tsegmb";
        info.detail = "TSEGMB via WinRing0 PCI config";
      }
    }
    ::CloseHandle(h);
  }
  return info;
#else
  return Result<SmramInfo>({}, "SMRAM discovery requires Linux or Windows");
#endif
}

Result<bool> is_smram_address(std::uint64_t phys_addr) {
  auto info = discover_smram();
  if (!info) return Result<bool>(false, info.error_msg);
  if (info->base == 0 || info->size == 0) {
    return Result<bool>(false, "SMRAM range unknown");
  }
  return phys_in_range(phys_addr, info->base, info->size);
}

}  // namespace real::smm
