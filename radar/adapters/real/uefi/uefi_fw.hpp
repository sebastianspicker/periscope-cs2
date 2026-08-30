// uefi_fw.hpp — UEFI firmware table scanning and ACPI walk.
//
// Walks RSDP -> XSDT/RSDT -> all ACPI tables and UEFI configuration
// table entries. Used for DMA remapping detection, TXT boot analysis,
// SMM communication, and physical memory layout discovery.
//
// Reference: UEFI 2.9 §4.6 ACPI Tables, Intel TXT Spec §3

#pragma once

#include "real/uefi/uefi_types.hpp"
#include "real/error.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace real::uefi {

/// Find the RSDP (Root System Description Pointer) by scanning
/// physical memory in the EBDA and BIOS areas, or OS firmware APIs.
Result<uint64_t> find_rsdp_pa();

/// Read and validate the RSDP from a physical address.
Result<AcpiRsdp> read_rsdp(uint64_t rsdp_pa);

/// Pure parse: validate RSDP bytes (checksum + signature). Drives shipped logic.
Result<AcpiRsdp> parse_rsdp(const uint8_t* data, size_t size);

/// Enumerate all ACPI SDTs (DSDT, SSDT, MCFG, DMAR, etc.) from XSDT/RSDT
/// or OS firmware table enumeration when rsdp_pa is kOsMediatedRsdpPa.
struct AcpiTableEntry {
    std::string signature;
    uint64_t    phys_addr{};
    uint32_t    size{};
};
Result<std::vector<AcpiTableEntry>> enumerate_acpi_tables(uint64_t rsdp_pa);

/// Pure parse of RSDT/XSDT body: extract entry physical addresses.
/// `sdt` points at full table including AcpiSdtHeader.
Result<std::vector<uint64_t>> parse_sdt_entry_addresses(const uint8_t* sdt,
                                                        size_t sdt_len,
                                                        bool is_xsdt);

/// Find a specific ACPI table by signature (e.g., "DMAR", "DSDT", "SSDT").
Result<AcpiTableEntry> find_acpi_table(const std::string& signature);

/// Read the UEFI System Table from known physical address / systab.
Result<EfiSystemTable> read_efi_system_table(uint64_t rsdp_pa);

/// Pure parse of an EFI_SYSTEM_TABLE buffer (minimum header + fields).
Result<EfiSystemTable> parse_efi_system_table(const uint8_t* data, size_t size);

/// Enumerate configuration table entries from the UEFI system table.
struct ConfigTableEntry {
    uint64_t vendor_guid_hi;
    uint64_t vendor_guid_lo;
    uint64_t table_pa;       // Physical address of the config table
};
Result<std::vector<ConfigTableEntry>> enumerate_config_table(
    const EfiSystemTable& st);

/// Pure parse of EFI_CONFIGURATION_TABLE array.
Result<std::vector<ConfigTableEntry>> parse_config_table_array(
    const uint8_t* data, size_t size, size_t entry_count);

/// Find a configuration table by GUID halves.
Result<uint64_t> find_config_table(const EfiSystemTable& st,
                                    uint64_t guid_hi, uint64_t guid_lo);

/// Read the DMA Remapping (DMAR) table for IOMMU configuration.
struct DmarInfo {
    uint8_t host_address_width{};
    bool    dma_remap_active{};
    int     drhd_count{};
    int     rmrr_count{};
    bool    include_all_pci{};   // any DRHD with INCLUDE_PCI_ALL
    uint64_t base_address{};     // First DRHD register base
    std::string detail;
};
Result<DmarInfo> read_dmar_table();

/// Pure DMAR parser over full table bytes (header + remapping structures).
Result<DmarInfo> parse_dmar_table(const uint8_t* data, size_t size);

/// Read the IVRS (I/O Virtualization Reporting Structure for AMD).
Result<DmarInfo> read_ivrs_table();

/// Pure IVRS parser.
Result<DmarInfo> parse_ivrs_table(const uint8_t* data, size_t size);

/// Enumerate all UEFI memory descriptors via OS map or buffer.
struct MemoryRegion {
    uint64_t phys_start{};
    uint64_t phys_end{};
    uint64_t num_pages{};
    uint32_t type{};           // EfiMemoryType
    uint64_t attributes{};
    bool     is_available{};
    bool     is_mmio{};
    bool     is_runtime{};
};
Result<std::vector<MemoryRegion>> get_memory_map(uint64_t rsdp_pa);

/// Pure parse of EFI memory map buffer (array of EfiMemoryDescriptor-sized records).
Result<std::vector<MemoryRegion>> parse_efi_memory_map(const uint8_t* data,
                                                       size_t size,
                                                       size_t descriptor_size);

} // namespace real::uefi
