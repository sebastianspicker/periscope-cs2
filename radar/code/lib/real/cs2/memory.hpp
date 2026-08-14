// memory.hpp — Real CS2 process memory reader.
//
// LESSON: Reading game memory from an external process is the core
// operation of T0 radar cheats. Every read leaves a telemetry scar:
//   - ReadProcessMemory calls are observable via API monitoring
//   - Direct syscalls bypass usermode hooks but not ETW/kernel callbacks
//   - Kernel reads (IOCTL) leave driver/device artifacts
//   - HV reads leave hypervisor artifacts
//   - DMA reads leave PCIe/IOMMU artifacts
//
// This file provides a unified memory reader that dispatches to the
// appropriate tier backend: RPM (T0), syscall (T1), IOCTL (T2),
// hypercall (T3), or DMA (T4).
//
// Educational design:
//   REAL MODE:   Reads real CS2 process memory via the selected backend.
//   SIM MODE:    Reads sim::World process memory via the lab backend.
//   Both modes teach the same memory-acquisition lesson.

#pragma once

#include "ac/memory_backend.hpp"
#include "ac/types.hpp"
#include "real/cs2/offsets.hpp"
#include "real/error.hpp"
#include "real/mode/mode.hpp"
#include "real/platform.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace real::cs2 {

/// Backend preference: controls fallback behavior in attach().
/// When PreferIoctl is set and the kernel IOCTL backend is available,
/// CS2 reads go through the kernel driver ONLY — never RPM.
/// RPM is only used for T0/T1 tiers where no kernel driver exists.
///
/// V4-V1 / V4-V2 rationale:
///   RpmBackend duplicates a VM_READ handle into our process (visible
///   in handle table enumeration — V4-V1) and calls
///   NtReadVirtualMemory which fires ETW Threat Intelligence events
///   (V4-V2). The kernel IOCTL path avoids both: no VM_READ handle
///   and no NtReadVirtualMemory syscall from usermode.
enum class MemoryBackendPreference {
    PreferIoctl,  // Use kernel IOCTL when available (T2+)
    PreferRpm     // Use user-mode RPM (T0/T1 only)
};

/// Tier-specific memory reader that implements ac::IMemoryBackend.
/// Created by MemoryReaderFactory based on the requested tier and mode.
/// Automatically falls back through tiers when a backend is unavailable.
/// When PreferIoctl is active, the IOCTL path is used for CS2 reads
/// and RPM fallback is blocked — see MemoryBackendPreference.
class Cs2MemoryReader {
public:
  Cs2MemoryReader();
  ~Cs2MemoryReader();

  /// Not copyable.
  Cs2MemoryReader(const Cs2MemoryReader&) = delete;
  Cs2MemoryReader& operator=(const Cs2MemoryReader&) = delete;

  /// Attach to the CS2 process with a specific tier backend.
  /// - T0: OpenProcess + ReadProcessMemory (simplest, most detectable)
  /// - T1: Direct syscall NtReadVirtualMemory (bypasses usermode hooks)
  /// - T2: IOCTL via kernel driver (no usermode game handle)
  /// - T3: Hypervisor hypercall (no kernel involvement)
  /// - T4: DMA physical memory read (off-box, no local process)
  ac::Status attach(ac::Tier tier, std::uint32_t pid);

  /// Detach from the process.
  void detach();

  /// Read memory at a virtual address in the CS2 process.
  ac::ReadResult read(std::uint64_t address, std::size_t size);

  /// Read memory at a virtual address with automatic dereference.
  template <typename T>
  ac::ReadResult read_struct(std::uint64_t address) {
    return read(address, sizeof(T));
  }

  /// Read a null-terminated string from CS2 memory.
  ac::ReadResult read_string(std::uint64_t address, std::size_t max_len = 256);

  /// Read a multi-level pointer chain.
  /// E.g., read_pointer_chain(base, {0x10, 0x20, 0x30}) reads
  /// *(*(*(base+0x10)+0x20)+0x30)
  Result<std::uint64_t> read_pointer_chain(
      std::uint64_t base,
      const std::vector<std::uint64_t>& offsets);

  /// Get the currently active tier name.
  const char* active_tier_name() const;

  /// Get the currently active backend name.
  const char* active_backend_name() const;

  /// Check if attached.
  bool is_attached() const { return attached_; }

  /// Set memory backend preference.
  /// When PreferIoctl is set, the kernel IOCTL path is preferred for
  /// CS2 reads and RPM fallback is disabled. Call before attach().
  void set_preference(MemoryBackendPreference pref) { m_preference = pref; }

  /// Get current memory backend preference.
  MemoryBackendPreference preference() const { return m_preference; }

private:
  bool attached_ = false;
  std::uint32_t pid_ = 0;
  ac::Tier active_tier_ = ac::Tier::T0_UsermodeRpm;
  MemoryBackendPreference m_preference = MemoryBackendPreference::PreferRpm;
  std::unique_ptr<ac::IMemoryBackend> backend_;
};

/// Create the best available memory backend for the given tier.
std::unique_ptr<ac::IMemoryBackend> create_backend(
    ac::Tier tier, std::uint32_t pid);

/// Create a memory backend with automatic fallback.
/// Tries the requested tier, then falls back through lower tiers.
std::unique_ptr<ac::IMemoryBackend> create_backend_with_fallback(
    ac::Tier preferred);

}  // namespace real::cs2
