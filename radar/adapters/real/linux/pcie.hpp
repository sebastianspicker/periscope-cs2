// pcie.hpp — Linux PCIe device enumeration, BAR mmap, config space, port I/O.

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"
#include "real/linux/pci_parse.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace real::linux::pcie {

struct PciDevice {
    pci::Bdf bdf{};
    uint8_t bus = 0;
    uint8_t device = 0;
    uint8_t function = 0;
    uint16_t vendor_id = 0;
    uint16_t device_id = 0;
    uint8_t class_code = 0;
    uint8_t subclass = 0;
    uint8_t prog_if = 0;
    uint8_t revision = 0;
    std::string driver;
    std::array<pci::BarResource, 6> bars{};
    uint64_t bar0 = 0;
    uint64_t bar0_size = 0;
    uint64_t bar1 = 0;
    uint64_t bar1_size = 0;
    bool is_dma_capable = false;
    std::string sysfs_path;
    std::string iommu_group; // basename of iommu_group symlink, if any
    bool sriov_capable = false;
};

// ── Enumeration ────────────────────────────────────────────────────

Result<std::vector<PciDevice>> enumerate() noexcept;
Result<PciDevice> find(uint16_t vendor_id, uint16_t device_id) noexcept;
Result<PciDevice> find_bdf(const pci::Bdf& bdf) noexcept;

// ── BAR access (resourceN mmap) ────────────────────────────────────

Result<std::vector<uint8_t>> bar_read(const PciDevice& dev, unsigned bar_index,
                                      uint64_t offset, size_t size) noexcept;
Result<void> bar_write(const PciDevice& dev, unsigned bar_index, uint64_t offset,
                       const std::vector<uint8_t>& data) noexcept;

// Convenience BAR0 wrappers (legacy API).
Result<std::vector<uint8_t>> bar_read(const PciDevice& dev, uint64_t offset,
                                      size_t size) noexcept;
Result<void> bar_write(const PciDevice& dev, uint64_t offset,
                       const std::vector<uint8_t>& data) noexcept;

// ── Config space ───────────────────────────────────────────────────

Result<std::vector<uint8_t>> config_read(const PciDevice& dev, uint64_t offset,
                                         size_t size) noexcept;
Result<void> config_write(const PciDevice& dev, uint64_t offset,
                          const std::vector<uint8_t>& data) noexcept;

// ── Port I/O ───────────────────────────────────────────────────────

Result<std::vector<uint8_t>> port_read(uint16_t port, size_t size) noexcept;
Result<void> port_write(uint16_t port,
                        const std::vector<uint8_t>& data) noexcept;
Result<bool> ioport_accessible() noexcept;

// ── IOMMU ──────────────────────────────────────────────────────────

Result<std::string> iommu_group_for(const PciDevice& dev) noexcept;
Result<bool> iommu_enabled() noexcept;

// ── Pure helpers ───────────────────────────────────────────────────

/// Fill bar0/bar1 convenience fields + is_dma_capable from bars[].
void finalize_device(PciDevice& dev) noexcept;

} // namespace real::linux::pcie
