#include "real/cs2/memory.hpp"

#include "real/memory.hpp"
#include "real/kernel/ioctl_interface.hpp"
#include "real/win/syscall_helper.hpp"
#include "real/win/xorstr.hpp"
#include "real/win/api_table.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#endif
#include "real/cs2/hijack_reader.hpp"

namespace real::cs2 {
namespace {

class RpmBackend final : public ac::IMemoryBackend {
 public:
  explicit RpmBackend(ac::Tier actual_tier = ac::Tier::T0_UsermodeRpm) : actual_tier_(actual_tier) {}
  ac::Tier tier() const override { return actual_tier_; }
  std::string_view name() const override {
    return actual_tier_ == ac::Tier::T1_SyscallSoft ? "real_syscall" : "real_rpm";
  }

  ac::Status attach(std::uint32_t target_id) override {
    detach();
    if (target_id == 0) return ac::Status::InvalidArgument;
#if LR_PLATFORM_WINDOWS
    // Prefer hijack-duplicated handle (stealth T0 path).
    auto& hijack = real::cs2::hijack::g_Hijack();
    if (hijack.is_ready() && hijack.cs2_pid() == target_id) {
      handle_ = static_cast<HANDLE>(hijack.cs2_handle());
      owns_handle_ = false;
      pid_ = target_id;
      return ac::Status::Ok;
    }

    // Educational fallback: classic OpenProcess(PROCESS_VM_READ).
    // This creates the primary T0 scar (handle visible in the handle table).
    auto& api = real::win::g_Api();
    if (!api.resolved || !api.OpenProcess) {
      return ac::Status::Unavailable;
    }
    HANDLE h = api.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
                               FALSE, target_id);
    if (!h) {
      return ac::Status::Denied;
    }
    handle_ = h;
    owns_handle_ = true;
    pid_ = target_id;
    return ac::Status::Ok;
#else
    (void)target_id;
    return ac::Status::Unavailable;
#endif
  }

  void detach() override {
#if LR_PLATFORM_WINDOWS
    if (owns_handle_ && handle_) {
      auto& api = real::win::g_Api();
      if (api.resolved && api.CloseHandle) {
        api.CloseHandle(handle_);
      } else {
        ::CloseHandle(handle_);
      }
    }
    // Hijack-borrowed handles are not closed here.
    handle_ = nullptr;
    owns_handle_ = false;
    pid_ = 0;
#else
    pid_ = 0;
#endif
  }

  bool is_attached() const override {
#if LR_PLATFORM_WINDOWS
    return handle_ != nullptr;
#else
    return false;
#endif
  }

  ac::ReadResult read(const ac::ReadRequest& request) override {
    if (!is_attached() || request.size == 0) {
      return {is_attached() ? ac::Status::InvalidArgument : ac::Status::Unavailable, {}};
    }
    if (request.address > std::numeric_limits<std::uint64_t>::max() - request.size) {
      return {ac::Status::InvalidArgument, {}};
    }
#if LR_PLATFORM_WINDOWS
    std::vector<std::uint8_t> bytes(request.size);
    SIZE_T bytes_read = 0;

    // Prefer indirect syscall (stealth path); fall back to resolved NT API /
    // ReadProcessMemory for the educational lab when SSN/gadget resolution fails.
    NTSTATUS status = real::win::syscall_direct_NtReadVirtualMemory(
        handle_, reinterpret_cast<PVOID>(request.address), bytes.data(),
        request.size, &bytes_read);
    if (status < 0) {
      auto& api = real::win::g_Api();
      if (api.resolved && api.NtReadVirtualMemory) {
        status = api.NtReadVirtualMemory(
            handle_, reinterpret_cast<PVOID>(request.address), bytes.data(),
            request.size, &bytes_read);
      }
    }
    if (status < 0) {
      auto& api = real::win::g_Api();
      if (api.resolved && api.ReadProcessMemory) {
        BOOL ok = api.ReadProcessMemory(
            handle_, reinterpret_cast<LPCVOID>(request.address), bytes.data(),
            request.size, &bytes_read);
        if (!ok) return {ac::Status::Denied, {}};
      } else {
        return {ac::Status::Denied, {}};
      }
    }
    bytes.resize(bytes_read);
    return {ac::Status::Ok, std::move(bytes)};
#else
    return {ac::Status::Unavailable, {}};
#endif
  }

 private:
#if LR_PLATFORM_WINDOWS
  HANDLE handle_ = nullptr;
  bool owns_handle_ = false;
#endif
  ac::Tier actual_tier_;
  std::uint32_t pid_ = 0;
};

/// T2+ kernel IOCTL memory reader.
///
/// V4-V1 / V4-V2 rationale:
///   RpmBackend (used by T0/T1) duplicates a VM_READ handle into our
///   process, making it visible in handle-table enumeration (V4-V1).
///   Its NtReadVirtualMemory call fires ETW Threat Intelligence events
///   (V4-V2). The IOCTL path avoids both:
///     - No VM_READ handle exists in our process handle table
///     - Memory reads happen inside kernel (MmCopyVirtualMemory),
///       never via NtReadVirtualMemory from usermode
///
/// This backend is available only when a compatible kernel driver
/// (e.g., gdrv.sys BYOVD) is loaded. attach() opens the device and
/// sends IOCTL_MEM_READ requests. If the device cannot be opened,
/// the backend reports Unavailable — it NEVER falls back to RPM
/// when Preference is PreferIoctl (see Cs2MemoryReader::attach).
class IoctlBackend final : public ac::IMemoryBackend {
 public:
  explicit IoctlBackend(const char* device_path = nullptr,
                          std::uint32_t ioctl_code = real::kernel::IOCTL_MEM_READ)
      : device_path_(device_path), ioctl_code_(ioctl_code) {}

  ac::Tier tier() const override { return ac::Tier::T2_KernelByovd; }
  std::string_view name() const override { return "real_ioctl"; }

  // Resolve device path: use OBF default if none provided
  const char* resolved_device_path() const noexcept {
    return device_path_ ? device_path_ : OBF("\\\\.\\gdrv");
  }

  ac::Status attach(std::uint32_t target_id) override {
    detach();
    if (target_id == 0) return ac::Status::InvalidArgument;
#if LR_PLATFORM_WINDOWS
    auto dev = real::kernel::open_device(resolved_device_path());
    if (!dev) return ac::Status::Unavailable;
    device_ = *dev;
    pid_ = target_id;
    attached_ = true;
    return ac::Status::Ok;
#else
    (void)target_id;
    return ac::Status::Unavailable;
#endif
  }

  void detach() override {
#if LR_PLATFORM_WINDOWS
    if (device_.native != 0) {
      (void)real::kernel::close_device(device_);
      device_ = {};
    }
#endif
    pid_ = 0;
    attached_ = false;
  }

  bool is_attached() const override {
    return attached_ && device_.native != 0;
  }

  ac::ReadResult read(const ac::ReadRequest& request) override {
    if (!is_attached() || request.size == 0) {
      return {is_attached() ? ac::Status::InvalidArgument : ac::Status::Unavailable, {}};
    }
    if (request.address > std::numeric_limits<std::uint64_t>::max() - request.size) {
      return {ac::Status::InvalidArgument, {}};
    }
#if LR_PLATFORM_WINDOWS
    // Pack IOCTL input: PID + address + size
    // The kernel driver extracts these to call MmCopyVirtualMemory.
    std::vector<std::uint8_t> input(sizeof(std::uint32_t) +
                                     sizeof(std::uint64_t) +
                                     sizeof(std::uint32_t));
    std::size_t offset = 0;
    std::memcpy(input.data() + offset, &pid_, sizeof(pid_));
    offset += sizeof(pid_);
    std::memcpy(input.data() + offset, &request.address, sizeof(request.address));
    offset += sizeof(request.address);
    std::uint32_t size32 = static_cast<std::uint32_t>(request.size);
    std::memcpy(input.data() + offset, &size32, sizeof(size32));

    auto result = real::kernel::send_ioctl(
        device_, ioctl_code_, input, request.size);
    if (!result) {
      // IOCTL failed — device may have been unloaded or handle revoked.
      // Do NOT fall back to RPM: the preference is PreferIoctl.
      return {ac::Status::Denied, {}};
    }
    return {ac::Status::Ok, std::move(result.value)};
#else
    return {ac::Status::Unavailable, {}};
#endif
  }

 private:
  // Device path is stored as raw pointer — it's either a static OBF string
  // or a caller-provided string that outlives this backend.
  const char* device_path_;
  std::uint32_t ioctl_code_;
#if LR_PLATFORM_WINDOWS
  real::kernel::DeviceHandle device_{};
#endif
  std::uint32_t pid_ = 0;
  bool attached_ = false;
};

/// Privileged-tier backend: hands off to real/vmx or real/dma when those
/// libraries are linked; otherwise fails with Unavailable and never pretends
/// to be attached or to return successful reads.
class UnavailableBackend final : public ac::IMemoryBackend {
 public:
  explicit UnavailableBackend(ac::Tier tier, const char* backend_name)
      : tier_(tier), name_(backend_name ? backend_name
                                        : "unavailable_requires_privileged_backend") {}

  ac::Tier tier() const override { return tier_; }
  std::string_view name() const override { return name_; }
  ac::Status attach(std::uint32_t target_id) override {
    // Explicit non-Ok: no silent attach, no fabricated RPM path.
    return target_id == 0 ? ac::Status::InvalidArgument : ac::Status::Unavailable;
  }
  void detach() override {}
  bool is_attached() const override { return false; }
  ac::ReadResult read(const ac::ReadRequest&) override {
    return {ac::Status::Unavailable, {}};
  }

 private:
  ac::Tier tier_;
  const char* name_;
};

}  // namespace

Cs2MemoryReader::Cs2MemoryReader() = default;

Cs2MemoryReader::~Cs2MemoryReader() {
  detach();
}

ac::Status Cs2MemoryReader::attach(ac::Tier tier, std::uint32_t pid) {
  detach();
  if (pid == 0) return ac::Status::InvalidArgument;

  // V5-T1: Automatically prefer IOCTL (kernel) path for T2+ tiers.
  // T0/T1 can only use RPM (user-mode), which creates handle table and
  // ETW artifacts. T2+ avoids both via kernel driver IOCTL path.
  if (tier >= ac::Tier::T2_KernelByovd) {
    m_preference = MemoryBackendPreference::PreferIoctl;
  }

  backend_ = create_backend(tier, pid);
  ac::Status status = backend_ ? backend_->attach(pid) : ac::Status::Unavailable;
  if (status != ac::Status::Ok) {
    if (m_preference == MemoryBackendPreference::PreferIoctl) {
      // V4-V1 / V4-V2 BLOCK: When PreferIoctl is set, NEVER fall back
      // to RpmBackend. RpmBackend creates a VM_READ handle visible in
      // handle-table enumeration (V4-V1) and calls NtReadVirtualMemory
      // which fires ETW Threat Intelligence events (V4-V2).
      // The kernel IOCTL path avoids both. If the kernel driver is
      // unavailable, we fail rather than risk detection.
      //
      // RPM fallback is only safe for T0/T1 educational demos where
      // handle-table enumeration and ETW telemetry are acceptable.
      backend_.reset();
      return status;
    }
    // Default (PreferRpm): fall back to user-mode RPM for T0/T1.
    // This is suitable for educational/demo use where detection
    // vectors V4-V1 and V4-V2 are understood and accepted.
    if (tier != ac::Tier::T0_UsermodeRpm) {
      backend_ = std::make_unique<RpmBackend>(ac::Tier::T0_UsermodeRpm);
      status = backend_ ? backend_->attach(pid) : ac::Status::Unavailable;
    }
  }
  if (status != ac::Status::Ok) {
    backend_.reset();
    return status;
  }

  attached_ = true;
  pid_ = pid;
  active_tier_ = backend_->tier();
  return ac::Status::Ok;
}

void Cs2MemoryReader::detach() {
  if (backend_) backend_->detach();
  backend_.reset();
  attached_ = false;
  pid_ = 0;
  active_tier_ = ac::Tier::T0_UsermodeRpm;
}

ac::ReadResult Cs2MemoryReader::read(std::uint64_t address, std::size_t size) {
  if (!attached_ || !backend_) return {ac::Status::Unavailable, {}};
  return backend_->read({address, size});
}

ac::ReadResult Cs2MemoryReader::read_string(std::uint64_t address, std::size_t max_len) {
  if (max_len == 0) return {ac::Status::InvalidArgument, {}};
  const auto result = read(address, max_len);
  if (result.status != ac::Status::Ok) return result;

  ac::ReadResult string = result;
  const auto terminator = std::find(string.bytes.begin(), string.bytes.end(), std::uint8_t{0});
  if (terminator != string.bytes.end()) string.bytes.resize(terminator - string.bytes.begin());
  return string;
}

Result<std::uint64_t> Cs2MemoryReader::read_pointer_chain(
    std::uint64_t base, const std::vector<std::uint64_t>& offsets) {
  std::uint64_t current = base;
  for (const std::uint64_t offset : offsets) {
    if (current > std::numeric_limits<std::uint64_t>::max() - offset) {
      return {0, "Pointer-chain address overflow"};
    }
    const auto result = read(current + offset, sizeof(current));
    if (result.status != ac::Status::Ok || result.bytes.size() != sizeof(current)) {
      return {0, "Pointer-chain read failed"};
    }
    std::memcpy(&current, result.bytes.data(), sizeof(current));
    if (current == 0) return {0, "Pointer-chain contains a null pointer"};
  }
  return Result<std::uint64_t>(current);
}

const char* Cs2MemoryReader::active_tier_name() const {
  if (!backend_) return "detached";
  switch (active_tier_) {
    case ac::Tier::T0_UsermodeRpm: return "t0_usermode_rpm";
    case ac::Tier::T1_SyscallSoft: return "t1_syscall_soft";
    case ac::Tier::T2_KernelByovd: return "t2_kernel_byovd";
    case ac::Tier::T3_Hypervisor: return "t3_hypervisor";
  }
  return "unknown";
}

const char* Cs2MemoryReader::active_backend_name() const {
  return backend_ ? backend_->name().data() : "detached";
}

std::unique_ptr<ac::IMemoryBackend> create_backend(ac::Tier tier, std::uint32_t pid) {
  (void)pid;
  switch (tier) {
    case ac::Tier::T0_UsermodeRpm:
      return std::make_unique<RpmBackend>(ac::Tier::T0_UsermodeRpm);
    case ac::Tier::T1_SyscallSoft:
      // T1 uses the same RpmBackend path (direct syscall NtReadVirtualMemory)
      // but with a different tier label. Still subject to V4-V1/V4-V2.
      return std::make_unique<RpmBackend>(ac::Tier::T1_SyscallSoft);
    case ac::Tier::T2_KernelByovd:
      // T2: kernel IOCTL — no VM_READ handle, no usermode NtReadVirtualMemory.
      // This is the only path that avoids both V4-V1 (handle table)
      // and V4-V2 (ETW Threat Intelligence).
      return std::make_unique<IoctlBackend>();
    case ac::Tier::T3_Hypervisor:
      // T3 lives in real/vmx (hypercall / EPT). That stack requires CPL=0 and
      // is not silently substituted with usermode RPM. PreferIoctl attach()
      // also refuses RPM fallback for T2+ — same honesty for T3.
      // When LR_ENABLE_REAL_VMX is ON, link ac_real_vmx and replace this with
      // a thin wrapper around hyperv_read_virtual_memory; until then fail clean.
      return std::make_unique<UnavailableBackend>(
          tier, "t3_hypervisor_requires_real_vmx_backend");
  }
  return nullptr;
}

std::unique_ptr<ac::IMemoryBackend> create_backend_with_fallback(ac::Tier preferred) {
  // Attachment needs a process id, so runtime fallback is completed by Cs2MemoryReader::attach.
  if (auto backend = create_backend(preferred, 0)) return backend;
  return create_backend(ac::Tier::T0_UsermodeRpm, 0);
}

}  // namespace real::cs2
