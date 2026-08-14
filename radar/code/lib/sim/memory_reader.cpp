#include "sim/memory_reader.hpp"

#include <algorithm>
#include <cstring>

namespace sim {
namespace {

bool has_handle_from(const World& world, std::uint32_t owner_pid,
                     std::uint32_t target_pid) {
  return std::any_of(world.handles.begin(), world.handles.end(),
                     [owner_pid, target_pid](const Handle& handle) {
                       return handle.owner_pid == owner_pid &&
                              handle.target_pid == target_pid &&
                              has(handle.access, AccessMask::VmRead);
                     });
}

ac::Status copy_read_result(const ac::ReadResult& result, void* buf,
                            std::size_t size) {
  if (result.status != ac::Status::Ok) {
    return result.status;
  }
  if (size != 0 && buf == nullptr) {
    return ac::Status::InvalidArgument;
  }
  const auto copy_size = std::min(size, result.bytes.size());
  if (copy_size != 0) {
    std::memcpy(buf, result.bytes.data(), copy_size);
  }
  return ac::Status::Ok;
}

ac::Status copy_bytes(const std::vector<std::uint8_t>& bytes, void* buf,
                      std::size_t size) {
  if (size != 0 && buf == nullptr) {
    return ac::Status::InvalidArgument;
  }
  const auto copy_size = std::min(size, bytes.size());
  if (copy_size != 0) {
    std::memcpy(buf, bytes.data(), copy_size);
  }
  return ac::Status::Ok;
}

}  // namespace

bool acquisition_compatible(ac::MemoryAcquisitionModel model,
                            ac::HandleAcquisitionModel handle_model,
                            ac::DefenseLayer layer) {
  switch (layer) {
    case ac::DefenseLayer::ProcessIsolation:
      // External (non-injected) paths are isolation-compatible.
      // Kernel/HV/DMA are also external to the game process address space.
      return true;
    case ac::DefenseLayer::ProxyMemoryAccess:
      // Only true proxy/hijack acquisition avoids a radar-owned handle.
      return model == ac::MemoryAcquisitionModel::HijackProxy ||
             handle_model == ac::HandleAcquisitionModel::HijackProxy;
    case ac::DefenseLayer::HardwareMonitorDisguise:
    case ac::DefenseLayer::ForensicTraceRemoval:
    case ac::DefenseLayer::PeLegitimacy:
    case ac::DefenseLayer::SystemNormalization:
    case ac::DefenseLayer::BehavioralJitter:
      return true;
  }
  return true;
}

void activate_acquisition(World& world, ac::MemoryAcquisitionModel model,
                          ac::HandleAcquisitionModel handle_model) {
  world.active_memory_model = model;
  world.active_handle_model = handle_model;
  world.note(std::string("acquisition active mem=") +
             std::string(ac::to_string(model)) +
             " handle=" + std::string(ac::to_string(handle_model)));
}

bool MemoryReader::compatible_with(ac::DefenseLayer layer) const {
  return acquisition_compatible(model(), handle_model(), layer);
}

// ── DirectRpmReader ─────────────────────────────────────────────────────────

DirectRpmReader::DirectRpmReader(World& world, std::uint32_t cheat_pid)
    : world_(world), cheat_pid_(cheat_pid) {
  activate_acquisition(world_, model(), handle_model());
}

ac::MemoryAcquisitionModel DirectRpmReader::model() const {
  return ac::MemoryAcquisitionModel::DirectRpm;
}

ac::HandleAcquisitionModel DirectRpmReader::handle_model() const {
  return ac::HandleAcquisitionModel::DirectOpenProcess;
}

ac::Status DirectRpmReader::read(std::uint64_t addr, void* buf,
                                 std::size_t size) {
  const auto game_pid = world_.game_pid();
  if (game_pid == 0) {
    return ac::Status::Unavailable;
  }
  if (!world_.open_process(cheat_pid_, game_pid, AccessMask::VmRead, false)) {
    return ac::Status::Unavailable;
  }
  return copy_read_result(world_.read_mem(cheat_pid_, game_pid, addr, size, true),
                          buf, size);
}

bool DirectRpmReader::has_handle_to(std::uint32_t target_pid) const {
  return has_handle_from(world_, cheat_pid_, target_pid);
}

bool DirectRpmReader::compatible_with(ac::DefenseLayer layer) const {
  return acquisition_compatible(model(), handle_model(), layer);
}

// ── SyscallReader ───────────────────────────────────────────────────────────

SyscallReader::SyscallReader(World& world, std::uint32_t cheat_pid)
    : world_(world), cheat_pid_(cheat_pid) {
  activate_acquisition(world_, model(), handle_model());
}

ac::MemoryAcquisitionModel SyscallReader::model() const {
  return ac::MemoryAcquisitionModel::Syscall;
}

ac::HandleAcquisitionModel SyscallReader::handle_model() const {
  return ac::HandleAcquisitionModel::DirectSyscall;
}

ac::Status SyscallReader::read(std::uint64_t addr, void* buf,
                               std::size_t size) {
  const auto game_pid = world_.game_pid();
  if (game_pid == 0) {
    return ac::Status::Unavailable;
  }
  if (!world_.open_process(cheat_pid_, game_pid, AccessMask::VmRead, true)) {
    return ac::Status::Unavailable;
  }
  world_.stack_spoof_on_read =
      world_.stack_spoof_on_read || world_.enhanced_stack_spoof;
  return copy_read_result(world_.read_mem(cheat_pid_, game_pid, addr, size, true),
                          buf, size);
}

bool SyscallReader::has_handle_to(std::uint32_t target_pid) const {
  return has_handle_from(world_, cheat_pid_, target_pid);
}

bool SyscallReader::compatible_with(ac::DefenseLayer layer) const {
  return acquisition_compatible(model(), handle_model(), layer);
}

// ── KernelIoctlReader ───────────────────────────────────────────────────────

KernelIoctlReader::KernelIoctlReader(World& world, std::uint32_t cheat_pid,
                                     std::string device_name)
    : world_(world),
      cheat_pid_(cheat_pid),
      device_name_(std::move(device_name)) {
  activate_acquisition(world_, model(), handle_model());
}

ac::MemoryAcquisitionModel KernelIoctlReader::model() const {
  return ac::MemoryAcquisitionModel::KernelIoctl;
}

ac::HandleAcquisitionModel KernelIoctlReader::handle_model() const {
  return ac::HandleAcquisitionModel::KernelDriver;
}

bool KernelIoctlReader::ensure_bridge() {
  if (bridge_ready_) {
    return true;
  }
  // Prefer an existing mem-rw device; otherwise install a lab bridge scar.
  for (const auto& d : world_.devices) {
    if (d.name == device_name_ && d.mem_rw_ioctl) {
      bridge_ready_ = true;
      return true;
    }
  }
  bool has_driver = false;
  for (const auto& drv : world_.drivers) {
    if (drv.provides_mem_rw || drv.name == "lab_bridge.sys") {
      has_driver = true;
      break;
    }
  }
  if (!has_driver) {
    world_.load_driver(Driver{"lab_bridge.sys", "lab_bridge_hash", "LabCo",
                              false, /*byovd_known_bad=*/false, false,
                              /*is_bridge=*/true, /*provides_mem_rw=*/true, 80});
  }
  world_.create_device(Device{device_name_, "lab_bridge.sys", true});
  world_.physmem_device_open = true;
  bridge_ready_ = true;
  world_.note("kernel_ioctl bridge ready device=" + device_name_);
  return true;
}

ac::Status KernelIoctlReader::read(std::uint64_t addr, void* buf,
                                   std::size_t size) {
  if (size != 0 && buf == nullptr) {
    return ac::Status::InvalidArgument;
  }
  const auto game_pid = world_.game_pid();
  if (game_pid == 0) {
    return ac::Status::Unavailable;
  }
  if (!ensure_bridge()) {
    return ac::Status::Unavailable;
  }
  std::vector<std::uint8_t> out;
  if (!world_.device_ioctl_read(cheat_pid_, device_name_, game_pid, addr, size,
                               out)) {
    return ac::Status::Denied;
  }
  // Kernel path: no usermode handle scar from cheat_pid.
  ++world_.remote_read_ops;
  world_.remote_read_bytes += static_cast<std::uint32_t>(size);
  return copy_bytes(out, buf, size);
}

bool KernelIoctlReader::has_handle_to(std::uint32_t target_pid) const {
  // Kernel IOCTL path does not leave a usermode handle from cheat_pid.
  (void)target_pid;
  return false;
}

bool KernelIoctlReader::compatible_with(ac::DefenseLayer layer) const {
  return acquisition_compatible(model(), handle_model(), layer);
}

// ── HvHypercallReader ───────────────────────────────────────────────────────

HvHypercallReader::HvHypercallReader(World& world, std::uint32_t cheat_pid,
                                     std::string vendor)
    : world_(world), cheat_pid_(cheat_pid), vendor_(std::move(vendor)) {
  activate_acquisition(world_, model(), handle_model());
}

ac::MemoryAcquisitionModel HvHypercallReader::model() const {
  return ac::MemoryAcquisitionModel::HvHypercall;
}

ac::HandleAcquisitionModel HvHypercallReader::handle_model() const {
  return ac::HandleAcquisitionModel::None;
}

bool HvHypercallReader::ensure_hv() {
  if (world_.trust.personal_hv_active || world_.hypercall_read_active) {
    return true;
  }
  // Lab convenience: if VBS/HVCI block personal HV, arm hypercall scar instead
  // (models platform-HV misuse / lab residual without requiring full disable).
  if (world_.try_start_personal_hv(vendor_)) {
    return true;
  }
  world_.hypercall_read_active = true;
  world_.hypercall_vendor = vendor_;
  world_.note("hv_reader fallback hypercall_read_active vendor=" + vendor_);
  return true;
}

ac::Status HvHypercallReader::read(std::uint64_t addr, void* buf,
                                   std::size_t size) {
  if (size != 0 && buf == nullptr) {
    return ac::Status::InvalidArgument;
  }
  const auto game_pid = world_.game_pid();
  if (game_pid == 0) {
    return ac::Status::Unavailable;
  }
  if (!ensure_hv()) {
    return ac::Status::Denied;
  }
  std::vector<std::uint8_t> out;
  if (world_.trust.personal_hv_active) {
    if (!world_.hv_read(cheat_pid_, game_pid, addr, size, out)) {
      return ac::Status::Denied;
    }
  } else {
    // Hypercall residual path: same byte source, no guest handle.
    auto rr = world_.read_mem(0, game_pid, addr, size, /*require_handle=*/false);
    if (rr.status != ac::Status::Ok) {
      return rr.status;
    }
    out = std::move(rr.bytes);
    world_.note("hypercall_read ok target=" + std::to_string(game_pid) +
                " size=" + std::to_string(size));
  }
  ++world_.remote_read_ops;
  world_.remote_read_bytes += static_cast<std::uint32_t>(size);
  return copy_bytes(out, buf, size);
}

bool HvHypercallReader::has_handle_to(std::uint32_t target_pid) const {
  (void)target_pid;
  return false;
}

bool HvHypercallReader::compatible_with(ac::DefenseLayer layer) const {
  return acquisition_compatible(model(), handle_model(), layer);
}

// ── DmaPhysicalReader ───────────────────────────────────────────────────────

DmaPhysicalReader::DmaPhysicalReader(World& world, std::uint32_t cheat_pid)
    : world_(world), cheat_pid_(cheat_pid) {
  activate_acquisition(world_, model(), handle_model());
}

ac::MemoryAcquisitionModel DmaPhysicalReader::model() const {
  return ac::MemoryAcquisitionModel::DmaPhysical;
}

ac::HandleAcquisitionModel DmaPhysicalReader::handle_model() const {
  return ac::HandleAcquisitionModel::DmaPhysical;
}

void DmaPhysicalReader::arm_device(bool present, bool iommu_bypass) {
  world_.trust.dma_device_present = present;
  if (iommu_bypass) {
    world_.iommu_bypass_active = true;
    world_.iommu_bypass_confirmed = true;
    world_.iommu_state = IommuState::Bypassed;
  } else if (present) {
    // DMA works when IOMMU is off in the lab model.
    world_.trust.iommu_on = false;
    world_.iommu_state = IommuState::Disabled;
    world_.iommu_disabled = true;
  }
  world_.dma_enabled = present;
  world_.fpga_smart_dma_active = present;
  world_.note(std::string("dma arm present=") + (present ? "1" : "0") +
              " iommu_bypass=" + (iommu_bypass ? "1" : "0"));
}

ac::Status DmaPhysicalReader::read(std::uint64_t addr, void* buf,
                                   std::size_t size) {
  if (size != 0 && buf == nullptr) {
    return ac::Status::InvalidArgument;
  }
  const auto game_pid = world_.game_pid();
  if (game_pid == 0) {
    return ac::Status::Unavailable;
  }
  if (!world_.trust.dma_device_present) {
    // Auto-arm a present device with IOMMU off for lab usability when caller
    // forgot; still requires explicit World trust path (not silent success
    // under IOMMU-on without bypass).
    arm_device(true, false);
  }
  std::vector<std::uint8_t> out;
  if (!world_.dma_read(game_pid, addr, size, out)) {
    return ac::Status::Denied;
  }
  ++world_.fpga_scatter_reads;
  ++world_.remote_read_ops;
  world_.remote_read_bytes += static_cast<std::uint32_t>(size);
  return copy_bytes(out, buf, size);
}

bool DmaPhysicalReader::has_handle_to(std::uint32_t target_pid) const {
  (void)target_pid;
  return false;
}

bool DmaPhysicalReader::compatible_with(ac::DefenseLayer layer) const {
  return acquisition_compatible(model(), handle_model(), layer);
}

// ── HijackProxyReader ───────────────────────────────────────────────────────

HijackProxyReader::HijackProxyReader(World& world, std::uint32_t cheat_pid)
    : world_(world), cheat_pid_(cheat_pid) {
  activate_acquisition(world_, model(), handle_model());
}

ac::MemoryAcquisitionModel HijackProxyReader::model() const {
  return ac::MemoryAcquisitionModel::HijackProxy;
}

ac::HandleAcquisitionModel HijackProxyReader::handle_model() const {
  return ac::HandleAcquisitionModel::HijackProxy;
}

ac::Status HijackProxyReader::read(std::uint64_t addr, void* buf,
                                   std::size_t size) {
  if (size != 0 && buf == nullptr) {
    return ac::Status::InvalidArgument;
  }
  if (donor_callback_) {
    return donor_callback_(addr, buf, size) ? ac::Status::Ok : ac::Status::Denied;
  }
  const auto game_pid = world_.game_pid();
  if (game_pid == 0) {
    return ac::Status::Unavailable;
  }
  // Prefer explicit donor; else use handle_proxy_owner or discover any proxy handle.
  std::uint32_t donor = donor_pid_;
  if (donor == 0) {
    donor = world_.handle_proxy_owner_pid;
  }
  if (donor == 0) {
    for (const auto& h : world_.handles) {
      if (h.target_pid == game_pid && h.via_proxy == false &&
          h.owner_pid != cheat_pid_ && has(h.access, AccessMask::VmRead)) {
        donor = h.owner_pid;
        break;
      }
    }
  }
  if (donor == 0 || !has_handle_from(world_, donor, game_pid)) {
    return ac::Status::Unavailable;
  }
  world_.handle_proxy_active = true;
  world_.handle_proxy_owner_pid = donor;
  world_.handle_proxy_consumer_pid = cheat_pid_;
  // Consumer has no handle; read is attributed to the donor owner.
  return copy_read_result(world_.read_mem(donor, game_pid, addr, size, true),
                          buf, size);
}

bool HijackProxyReader::has_handle_to(std::uint32_t target_pid) const {
  // Radar PID never owns the game handle under hijack proxy.
  (void)target_pid;
  return false;
}

bool HijackProxyReader::compatible_with(ac::DefenseLayer layer) const {
  return acquisition_compatible(model(), handle_model(), layer);
}

void HijackProxyReader::set_donor_callback(
    bool (*cb)(std::uint64_t, void*, std::size_t)) {
  donor_callback_ = cb;
}

void HijackProxyReader::set_donor_pid(std::uint32_t donor_pid) {
  donor_pid_ = donor_pid;
}

// ── Factory ─────────────────────────────────────────────────────────────────

std::unique_ptr<MemoryReader> CreateMemoryReader(
    ac::MemoryAcquisitionModel model, World& world, std::uint32_t cheat_pid) {
  switch (model) {
    case ac::MemoryAcquisitionModel::DirectRpm:
      return std::make_unique<DirectRpmReader>(world, cheat_pid);
    case ac::MemoryAcquisitionModel::Syscall:
      return std::make_unique<SyscallReader>(world, cheat_pid);
    case ac::MemoryAcquisitionModel::KernelIoctl:
      return std::make_unique<KernelIoctlReader>(world, cheat_pid);
    case ac::MemoryAcquisitionModel::HvHypercall:
      return std::make_unique<HvHypercallReader>(world, cheat_pid);
    case ac::MemoryAcquisitionModel::DmaPhysical:
      return std::make_unique<DmaPhysicalReader>(world, cheat_pid);
    case ac::MemoryAcquisitionModel::HijackProxy:
      return std::make_unique<HijackProxyReader>(world, cheat_pid);
  }
  return nullptr;
}

}  // namespace sim
