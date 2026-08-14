// uefi_dma.hpp — UEFI DMA remapping detection, IOMMU bypass, NT336 integration.
//
// DMA attacks via Thunderbolt, PCIe, or PCILeech hardware read physical
// memory directly. UEFI provides the DMAR/IVRS tables that describe the
// IOMMU configuration. Disabling DMA remapping at the UEFI level allows
// hardware-level memory access.

#pragma once

#include "real/uefi/uefi_types.hpp"
#include "real/error.hpp"

#include <cstdint>
#include <vector>

namespace real::uefi {

/// Check if the platform has active DMA remapping via DMAR/IVRS.
/// Returns true if VT-d/AMD-Vi is active.
Result<bool> dma_remapping_active();

/// Describe the DMA protection level from UEFI configuration.
struct DmaProtectionInfo {
    bool    vt_d_active{};           // Intel VT-d with DMA remapping
    bool    amd_vi_active{};         // AMD-Vi
    bool    drhd_all_devices{};       // All devices covered by DRHD
    bool    iommu_bypass_available{}; // PCIe ACS bypass or IOMMU disabled
    bool    thunderbolt_active{};     // Thunderbolt DMA protection
    int     drhd_count{};            // Number of DMA remapping units
    uint8_t address_width{};         // Physical address width for remap
    std::string detail;
};
Result<DmaProtectionInfo> get_dma_protection_info();

/// Check for Thunderbolt DMA protection in UEFI configuration tables.
/// Thunderbolt port is DMA-capable unless the firmware sets
/// SecurityLevel in the Thunderbolt (VSEC) configuration.
Result<bool> thunderbolt_dma_available();

/// Find PCIe base address register configuration from MCFG table.
/// The MCFG (Memory-Mapped Configuration Space) table describes the
/// PCIe config space physical address range.
struct McfgInfo {
    uint64_t base_address{};
    uint16_t segment_group{};
    uint8_t  start_bus{};
    uint8_t  end_bus{};
};
Result<McfgInfo> read_mcfg_table();

/// Pure MCFG parser over full table bytes.
Result<McfgInfo> parse_mcfg_table(const uint8_t* data, size_t size);

/// Check if NT336 / PCILeech DMA device is present on PCIe bus.
/// Scans MCFG ECAM and/or OS PCI enumeration for vendor 0x10EE (Xilinx).
Result<bool> fpga_dma_device_present();

} // namespace real::uefi
