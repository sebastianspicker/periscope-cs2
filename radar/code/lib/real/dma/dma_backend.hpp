// dma_backend.hpp — Real DMA (Direct Memory Access) backends.
// Supports physical memory read via PCIe device, Thunderbolt DMA,
// FPGA-based PCILeech-style operations, and IOMMU bypass.
//
// Pure logic (page walk, scatter descriptors) lives in page_walk.hpp and
// is unit-testable without live hardware.

#pragma once

#include "ac/memory_backend.hpp"
#include "real/dma/page_walk.hpp"
#include "real/error.hpp"
#include "real/platform.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace real::dma {

// ── PCIe Device Enumeration ────────────────────────────────────────

/// PCIe device location (bus:device:function).
struct PciAddress {
  std::uint8_t bus = 0;
  std::uint8_t device = 0;
  std::uint8_t function = 0;

  bool operator==(const PciAddress& o) const {
    return bus == o.bus && device == o.device && function == o.function;
  }
};

/// Describes a PCIe device found on the bus.
struct PciDevice {
  PciAddress address;
  std::uint16_t vendor_id = 0;
  std::uint16_t device_id = 0;
  std::string driver;
  std::uint64_t bar0 = 0;     // BAR0 physical address
  std::size_t bar0_size = 0;
  std::uint64_t bar1 = 0;
  std::size_t bar1_size = 0;
  bool is_dma_capable = false;
  std::uint8_t class_code = 0;   // base class (e.g. 0x06 = bridge)
  std::uint8_t subclass = 0;     // subclass (e.g. 0x04 = PCI-to-PCI bridge)
};

/// Enumerate all PCIe devices on the system.
Result<std::vector<PciDevice>> enum_pci_devices();

/// Find a PCIe device by vendor/device ID.
Result<PciDevice> find_pci_device(std::uint16_t vendor_id,
                                   std::uint16_t device_id);

// ── IOMMU Detection ────────────────────────────────────────────────

/// Check if the IOMMU (VT-d on Intel, AMD-Vi on AMD) is enabled.
Result<bool> iommu_enabled();

/// Attempt to bypass IOMMU via PCIe configuration space manipulation
/// (ACS Source Validation clear). Walks the full extended-capability chain.
Result<bool> iommu_bypass();

// ── Physical DMA Read ──────────────────────────────────────────────

/// Read physical memory via a DMA-capable PCIe device's BAR.
/// The BAR is memory-mapped persistently across calls (cached by BDF key).
/// The mapping is kept open to avoid the open/mmap/munmap/close cycle per read.
Result<std::vector<std::uint8_t>> pcie_bar_read(std::uint64_t phys_addr,
                                                  std::size_t size,
                                                  const PciDevice& device);

/// Read physical memory via \\.\PhysicalMemory (Windows) or /dev/mem (Linux).
Result<std::vector<std::uint8_t>> physmem_read(std::uint64_t phys_addr,
                                                 std::size_t size);

// ── Thunderbolt DMA ────────────────────────────────────────────────

/// Check if a Thunderbolt DMA device is accessible.
Result<bool> thunderbolt_available();

/// Attempt Thunderbolt DMA read.
Result<std::vector<std::uint8_t>> thunderbolt_dma_read(std::uint64_t phys_addr,
                                                        std::size_t size);

// ── FPGA / PCILeech-style DMA ──────────────────────────────────────

/// FPGA-based scatter-gather DMA (PCILeech pattern).
/// Reads memory in pages and handles physical address translation.
struct FpgaDmaDevice {
  std::string device_path;   // e.g., "/dev/fpga0" or "\\\\.\\FPGA0"
  std::uint64_t bar_base = 0;
  std::size_t bar_size = 0;
  int fd = -1;                  // opened file descriptor / handle cast
  bool use_device_io = false;   // true when Windows DeviceIoControl path is used
};

/// Open an FPGA DMA device.
Result<FpgaDmaDevice> fpga_dma_open(const std::string& device_path);

/// Perform a scatter-gather read through FPGA DMA.
/// Builds page-aligned descriptors via build_scatter_list and programs the
/// device ring (Linux pwrite or Windows DeviceIoControl).
Result<std::vector<std::uint8_t>> fpga_scatter_read(FpgaDmaDevice& dev,
                                                      std::uint64_t phys_addr,
                                                      std::size_t size);

/// Close the FPGA DMA device.
Result<void> fpga_dma_close(FpgaDmaDevice& dev);

// ── USB Device Firmware Upgrade (DFU) DMA ──────────────────────────

/// Check for USB DFU-class devices that might be used for DMA.
Result<bool> usb_dfu_dma_possible();

// ── ACPI PM Timer SMI / SMM DMA ───────────────────────────────────

/// Trigger an SMI via ACPI PM Timer to read physical memory through SMM.
Result<std::vector<std::uint8_t>> acpi_smi_read(std::uint64_t phys_addr,
                                                  std::size_t size);

// ── PCIe Peer-to-Peer DMA ──────────────────────────────────────────

/// Perform PCIe peer-to-peer DMA between two devices.
Result<std::vector<std::uint8_t>> pcie_peer_read(std::uint64_t phys_addr,
                                                   std::size_t size,
                                                   const PciDevice& reader,
                                                   const PciDevice& writer);

// ── Integrate with existing IMemoryBackend interface ───────────────

/// Real DMA backend implementing ac::IMemoryBackend.
///
/// Attach order (first success wins):
///   1. physmem_read probe
///   2. FPGA device (if use_fpga_ or auto-probe paths)
///   3. Thunderbolt BAR path
/// Detach always releases any open FPGA handle and clears state.
class RealDmaBackend final : public ac::IMemoryBackend {
public:
  explicit RealDmaBackend(bool use_fpga = false);

  // Tier enum maxes at T3; DMA identity is via name() and full transport chain.
  ac::Tier tier() const noexcept override { return ac::Tier::T0_UsermodeRpm; }
  std::string_view name() const noexcept override { return "real_dma"; }
  ac::Status attach(std::uint32_t target_id) override;
  void detach() override;
  bool is_attached() const noexcept override { return attached_; }
  ac::ReadResult read(const ac::ReadRequest& request) override;

  /// Which transport succeeded on attach (for diagnostics / tests).
  enum class Transport : std::uint8_t {
    None = 0,
    PhysMem,
    Fpga,
    Thunderbolt,
  };
  Transport active_transport() const noexcept { return transport_; }

private:
  bool attached_ = false;
  bool use_fpga_ = false;
  Transport transport_ = Transport::None;
  FpgaDmaDevice fpga_dev_{};
  std::uint32_t target_id_ = 0;
};

}  // namespace real::dma
