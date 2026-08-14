// smi.cpp — SMI trigger, CMOS bank access, SMM communicate, SMI telemetry.
//
// LESSON: SMM is the most privileged x86 mode. SMM code is invisible to the
// OS and hypervisor. Operations leave detectable artifacts (SMI count MSR,
// latency spikes, CMOS patterns, known IO-driver IOCTLs).

#include "real/smm/smm_interface.hpp"
#include "real/memory.hpp"
#include "real/platform.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

#if LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <unistd.h>
#elif LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#  include <intrin.h>
#endif

namespace real::smm {

// ── trigger_smi ────────────────────────────────────────────────────
//
// TECHNIQUE: Write to I/O port 0xB2 (or FADT SMI_CMD) to raise SW-SMI.
//
// SCAR: MSR_SMI_COUNT (0x34) increments; PM timer latency spike.
//
// BLUE: Sample SMI count + latency; unexpected SW-SMI storms.
//
// MITIGATION: SMM lockdown, SMM_Code_Chk_En, UEFI Secure Boot + STM.

Result<void> trigger_smi_on_port(std::uint16_t port, std::uint8_t smi_command) {
  std::printf("[smm] trigger_smi: port=0x%04x cmd=0x%02x\n", port, smi_command);
  return port_outb(port, smi_command);
}

Result<void> trigger_smi(std::uint8_t smi_command) {
  // Prefer FADT-advertised SMI_CMD when available.
  std::uint16_t port = kSmiCommandPort;
  auto fadt = read_fadt_smi_info();
  if (fadt && fadt->ok && fadt->smi_cmd != 0 && fadt->smi_cmd <= 0xFFFFu) {
    port = static_cast<std::uint16_t>(fadt->smi_cmd);
  }
  return trigger_smi_on_port(port, smi_command);
}

// ── CMOS ───────────────────────────────────────────────────────────
//
// TECHNIQUE: Index/data pair 0x70/0x71. NMI-disable bit on index writes.
//
// SCAR: CMOS access patterns; checksum changes after writes.
//
// BLUE: Audit CMOS checksum vs factory; rate-limit 0x70 traffic via HV.
//
// MITIGATION: Firmware CMOS lockdown; measured boot PCR covers config.

Result<void> smi_cmos_write(std::uint16_t index, std::uint8_t value) {
  if (index > 0xFF) return Result<void>("Invalid CMOS index");
  const std::uint8_t idx = static_cast<std::uint8_t>(index | kCmosNmiDisable);
  auto w1 = port_outb(kCmosIndexPort, idx);
  if (!w1) return w1;
  auto w2 = port_outb(kCmosDataPort, value);
  if (!w2) return w2;
  std::printf("[smm] CMOS write: index=0x%02x value=0x%02x\n", index, value);
  return Result<void>();
}

Result<std::uint8_t> smi_cmos_read(std::uint16_t index) {
  if (index > 0xFF) return Result<std::uint8_t>(0, "Invalid CMOS index");
  const std::uint8_t idx = static_cast<std::uint8_t>(index | kCmosNmiDisable);
  auto w = port_outb(kCmosIndexPort, idx);
  if (!w) return Result<std::uint8_t>(0, w.error_msg);
  auto r = port_inb(kCmosDataPort);
  if (r) {
    std::printf("[smm] CMOS read: index=0x%02x value=0x%02x\n", index, *r);
  }
  return r;
}

Result<std::vector<std::uint8_t>> cmos_dump(std::size_t length) {
  if (length == 0) return std::vector<std::uint8_t>{};
  if (length > 256) length = 256;
  std::vector<std::uint8_t> out(length);
  for (std::size_t i = 0; i < length; ++i) {
    auto b = smi_cmos_read(static_cast<std::uint16_t>(i));
    if (!b) return Result<std::vector<std::uint8_t>>({}, b.error_msg);
    out[i] = *b;
  }
  return out;
}

// ── smm_communicate ────────────────────────────────────────────────
//
// TECHNIQUE: Shared physical buffer + SW-SMI (UEFI SMM Communication pattern).
//
// SCAR: Buffer not in SMRAM is OS-visible; SMI count increments.
//
// BLUE: Scan for SMM magic in non-SMRAM pages; profile SMI sources.
//
// MITIGATION: TSEG lock, SMM_Code_Chk_En, STM, no unsigned SMM handlers.

Result<std::vector<std::uint8_t>> smm_communicate(
    std::uint64_t shared_phys_addr, std::size_t size,
    const std::vector<std::uint8_t>& request) {
  if (size < sizeof(SmmCommHeader)) {
    return Result<std::vector<std::uint8_t>>(
        {}, "SMM communication buffer is too small");
  }
  std::printf("[smm] smm_communicate: phys=0x%llx size=%zu req=%zu\n",
              static_cast<unsigned long long>(shared_phys_addr), size,
              request.size());

  auto mapped = map_physical(shared_phys_addr, size);
  if (!mapped) {
    return Result<std::vector<std::uint8_t>>({}, mapped.error_msg);
  }

  std::memset(mapped->base, 0, size);
  const std::size_t copy_size = std::min(request.size(), size);
  if (copy_size) std::memcpy(mapped->base, request.data(), copy_size);

  // Ensure request magic is present even if caller passed raw payload.
  auto* flag = reinterpret_cast<volatile std::uint32_t*>(mapped->base);
  if (copy_size < 4 || *flag != kSmmReqMagic) {
    *flag = kSmmReqMagic;
  }

  auto smi = trigger_smi(static_cast<std::uint8_t>(SmiCommand::Communicate));
  if (!smi) {
    unmap_physical(*mapped);
    return Result<std::vector<std::uint8_t>>(
        {}, std::string("SMI trigger failed: ") + smi.error_msg.c_str());
  }

  bool completed = false;
  for (int i = 0; i < 100000; ++i) {
    const std::uint32_t m = *flag;
    if (m == kSmmRspMagic || m == kSmmErrMagic) {
      completed = true;
      break;
    }
#if LR_PLATFORM_LINUX && (LR_ARCH_X64 || LR_ARCH_X86)
    __builtin_ia32_pause();
#elif LR_PLATFORM_WINDOWS && LR_ARCH_X64
    _mm_pause();
#endif
  }

  if (!completed) {
    unmap_physical(*mapped);
    return Result<std::vector<std::uint8_t>>({}, "SMM communication timeout");
  }

  std::vector<std::uint8_t> response(size);
  std::memcpy(response.data(), mapped->base, size);
  unmap_physical(*mapped);
  std::printf("[smm] SMM communication completed: %zu bytes\n", response.size());
  return response;
}

// ── SMI count / latency ────────────────────────────────────────────

Result<std::uint64_t> read_smi_count() {
#if LR_PLATFORM_LINUX
  // Prefer CPU0 MSR device.
  const char* paths[] = {"/dev/cpu/0/msr", "/dev/msr0"};
  int fd = -1;
  for (const char* p : paths) {
    fd = open(p, O_RDONLY);
    if (fd >= 0) break;
  }
  if (fd < 0) {
    auto err = os_error("open MSR device (need root + msr module)");
    return Result<std::uint64_t>(0, err.error_msg);
  }
  std::uint64_t value = 0;
  if (pread(fd, &value, sizeof(value), kMsrSmiCount) !=
      static_cast<ssize_t>(sizeof(value))) {
    close(fd);
    auto err = os_error("pread MSR_SMI_COUNT");
    return Result<std::uint64_t>(0, err.error_msg);
  }
  close(fd);
  std::printf("[smm] MSR_SMI_COUNT=0x%llx\n",
              static_cast<unsigned long long>(value));
  return value;
#elif LR_PLATFORM_WINDOWS
  // OpenLibSys WinRing0: IOCTL_OLS_READ_MSR variants differ by fork.
  const char* names[] = {"\\\\.\\WinRing0_1_2_0", "\\\\.\\WinRing0"};
  HANDLE h = INVALID_HANDLE_VALUE;
  for (const char* n : names) {
    h = ::CreateFileA(n, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                      0, nullptr);
    if (h != INVALID_HANDLE_VALUE) break;
  }
  if (h == INVALID_HANDLE_VALUE) {
    return Result<std::uint64_t>(0, "MSR read requires WinRing0-class lab driver");
  }
  DWORD ioctl_read_msr = 0x9C402084;  // OLS_READ_MSR in original WinRing0
  struct {
    ULONG reg;
  } in_req{kMsrSmiCount};
  std::uint64_t value = 0;
  DWORD ret = 0;
  BOOL ok = ::DeviceIoControl(h, ioctl_read_msr, &in_req, sizeof(in_req), &value,
                              sizeof(value), &ret, nullptr);
  if (!ok) {
    ioctl_read_msr = 0x9C402080;
    ok = ::DeviceIoControl(h, ioctl_read_msr, &in_req, sizeof(in_req), &value,
                           sizeof(value), &ret, nullptr);
  }
  ::CloseHandle(h);
  if (!ok) {
    auto err = os_error("DeviceIoControl READ_MSR");
    return Result<std::uint64_t>(0, err.error_msg);
  }
  return value;
#else
  return Result<std::uint64_t>(0, "MSR_SMI_COUNT requires x86 MSR access");
#endif
}

Result<std::uint64_t> measure_smi_latency_us(std::uint8_t smi_command) {
  using clock = std::chrono::steady_clock;
  const auto t0 = clock::now();
  auto r = trigger_smi(smi_command);
  const auto t1 = clock::now();
  if (!r) return Result<std::uint64_t>(0, r.error_msg);
  const auto us =
      std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
  std::printf("[smm] SMI latency: %lld us class=%d\n",
              static_cast<long long>(us),
              static_cast<int>(classify_smi_latency_us(
                  static_cast<std::uint64_t>(us < 0 ? 0 : us))));
  return static_cast<std::uint64_t>(us < 0 ? 0 : us);
}

}  // namespace real::smm
