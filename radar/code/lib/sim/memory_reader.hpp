#pragma once

// Memory acquisition abstraction. Each tier registers a different backend.
// Enforces that red strategies cannot mix incompatible acquisition + handle models.
// The factory creates the appropriate reader based on model.
// All operations are sim-only — they set World fields, never touch real OS.

#include "ac/types.hpp"
#include "sim/world.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sim {

/// Abstract memory reader — the single trust boundary for game memory access.
/// Each tier provides its own implementation.
class MemoryReader {
 public:
  virtual ~MemoryReader() = default;

  /// Which memory acquisition model this reader uses.
  virtual ac::MemoryAcquisitionModel model() const = 0;

  /// How this reader acquires (or avoids) handles to the target process.
  virtual ac::HandleAcquisitionModel handle_model() const = 0;

  /// Read |size| bytes from |addr| in the target process into |buf|.
  /// Returns Status::Ok on success, Status::Denied if the model forbids it.
  virtual ac::Status read(std::uint64_t addr, void* buf, std::size_t size) = 0;

  /// Whether this reader holds a handle to |target_pid|.
  virtual bool has_handle_to(std::uint32_t target_pid) const = 0;

  /// Whether this reader's acquisition model is compatible with the given
  /// defense layer. E.g., HijackProxy + ProxyMemoryAccess = compatible,
  /// DirectRpm + ProxyMemoryAccess = incompatible (creates own handle).
  virtual bool compatible_with(ac::DefenseLayer layer) const;

  /// Typed convenience read into a POD / aggregate |T|.
  template <typename T>
  ac::Status read_t(std::uint64_t addr, T& out) {
    return read(addr, &out, sizeof(T));
  }
};

/// Policy helper: acquisition vs defense-layer compatibility matrix.
bool acquisition_compatible(ac::MemoryAcquisitionModel model,
                            ac::HandleAcquisitionModel handle_model,
                            ac::DefenseLayer layer);

/// T0: Direct RPM via ReadProcessMemory / NtReadVirtualMemory.
class DirectRpmReader final : public MemoryReader {
 public:
  explicit DirectRpmReader(World& world, std::uint32_t cheat_pid);
  ac::MemoryAcquisitionModel model() const override;
  ac::HandleAcquisitionModel handle_model() const override;
  ac::Status read(std::uint64_t addr, void* buf, std::size_t size) override;
  bool has_handle_to(std::uint32_t target_pid) const override;
  bool compatible_with(ac::DefenseLayer layer) const override;

 private:
  World& world_;
  std::uint32_t cheat_pid_;
};

/// T1: Direct syscall (bypasses ntdll, but handle remains visible).
class SyscallReader final : public MemoryReader {
 public:
  explicit SyscallReader(World& world, std::uint32_t cheat_pid);
  ac::MemoryAcquisitionModel model() const override;
  ac::HandleAcquisitionModel handle_model() const override;
  ac::Status read(std::uint64_t addr, void* buf, std::size_t size) override;
  bool has_handle_to(std::uint32_t target_pid) const override;
  bool compatible_with(ac::DefenseLayer layer) const override;

 private:
  World& world_;
  std::uint32_t cheat_pid_;
};

/// T2: Kernel driver IOCTL (MmCopyVirtualMemory-class path). No usermode handle.
class KernelIoctlReader final : public MemoryReader {
 public:
  explicit KernelIoctlReader(World& world, std::uint32_t cheat_pid,
                             std::string device_name = "\\\\.\\LabMemRw");
  ac::MemoryAcquisitionModel model() const override;
  ac::HandleAcquisitionModel handle_model() const override;
  ac::Status read(std::uint64_t addr, void* buf, std::size_t size) override;
  bool has_handle_to(std::uint32_t target_pid) const override;
  bool compatible_with(ac::DefenseLayer layer) const override;

  /// Ensure bridge driver + device exist in the World (creates if missing).
  bool ensure_bridge();

 private:
  World& world_;
  std::uint32_t cheat_pid_;
  std::string device_name_;
  bool bridge_ready_ = false;
};

/// T3: Hypervisor hypercall / personal HV bridge. No usermode handle.
class HvHypercallReader final : public MemoryReader {
 public:
  explicit HvHypercallReader(World& world, std::uint32_t cheat_pid,
                             std::string vendor = "lab-hv");
  ac::MemoryAcquisitionModel model() const override;
  ac::HandleAcquisitionModel handle_model() const override;
  ac::Status read(std::uint64_t addr, void* buf, std::size_t size) override;
  bool has_handle_to(std::uint32_t target_pid) const override;
  bool compatible_with(ac::DefenseLayer layer) const override;

  /// Attempt personal HV start (fails closed under VBS/HVCI unless already active).
  bool ensure_hv();

 private:
  World& world_;
  std::uint32_t cheat_pid_;
  std::string vendor_;
};

/// T4: DMA physical memory read. No local handle, no kernel driver in guest.
class DmaPhysicalReader final : public MemoryReader {
 public:
  explicit DmaPhysicalReader(World& world, std::uint32_t cheat_pid = 0);
  ac::MemoryAcquisitionModel model() const override;
  ac::HandleAcquisitionModel handle_model() const override;
  ac::Status read(std::uint64_t addr, void* buf, std::size_t size) override;
  bool has_handle_to(std::uint32_t target_pid) const override;
  bool compatible_with(ac::DefenseLayer layer) const override;

  /// Mark DMA device present and optionally arm IOMMU-bypass scars.
  void arm_device(bool present = true, bool iommu_bypass = false);

 private:
  World& world_;
  std::uint32_t cheat_pid_;
};

/// Cross-cutting: hijack proxy reader. No handle from radar PID.
class HijackProxyReader final : public MemoryReader {
 public:
  explicit HijackProxyReader(World& world, std::uint32_t cheat_pid);
  ac::MemoryAcquisitionModel model() const override;
  ac::HandleAcquisitionModel handle_model() const override;
  ac::Status read(std::uint64_t addr, void* buf, std::size_t size) override;
  bool has_handle_to(std::uint32_t target_pid) const override;
  bool compatible_with(ac::DefenseLayer layer) const override;

  /// Register the donor callback that performs actual reads.
  void set_donor_callback(bool (*cb)(std::uint64_t, void*, std::size_t));

  /// Prefer donor-owned handle path through World when no callback is set.
  /// Reads via donor_pid's VmRead handle (require_handle=true on donor).
  void set_donor_pid(std::uint32_t donor_pid);

 private:
  World& world_;
  std::uint32_t cheat_pid_;
  std::uint32_t donor_pid_ = 0;
  bool (*donor_callback_)(std::uint64_t, void*, std::size_t) = nullptr;
};

/// Factory: create appropriate reader based on acquisition model.
/// Returns nullptr only for unknown model values.
std::unique_ptr<MemoryReader> CreateMemoryReader(
    ac::MemoryAcquisitionModel model,
    World& world,
    std::uint32_t cheat_pid);

/// Record the active acquisition model on the World scar surface.
void activate_acquisition(World& world, ac::MemoryAcquisitionModel model,
                          ac::HandleAcquisitionModel handle_model);

}  // namespace sim
