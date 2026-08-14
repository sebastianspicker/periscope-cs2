// uefi_lab.hpp — UEFI laboratory: firmware analysis, DMA bypass, and TXT measurement.
//
// Ties together the UEFI subsystem modules for educational demonstrations:
//   - Firmware table enumeration and analysis
//   - Secure Boot state inspection and variable manipulation
//   - DMA remapping (IOMMU) status and bypass assessment
//   - Intel TXT measured launch verification
//   - TPM event log parsing
//   - PCIe FPGA DMA device discovery

#pragma once

#include "real/uefi/uefi_types.hpp"
#include "real/uefi/uefi_fw.hpp"
#include "real/uefi/uefi_var.hpp"
#include "real/uefi/uefi_tpm.hpp"
#include "real/uefi/uefi_dma.hpp"
#include "real/error.hpp"

#include <string>

namespace real::uefi::lab {

/// Full firmware analysis report.
struct FirmwareReport {
    struct {
        bool found{};
        uint64_t pa{};
        int revision{};
    } rsdp;

    struct {
        int sdt_count{};
        bool has_dmar{};
        bool has_ivrs{};
        bool has_mcfg{};
        bool has_ssdt{};
        bool has_tpm2{};
        std::string detail;
    } acpi;

    struct {
        bool system_table_found{};
        uint64_t runtime_services_pa{};
        int config_table_entries{};
    } uefi;

    struct {
        bool active{};
        int drhd_count{};
        uint8_t address_width{};
        bool thunderbolt_dma{};
        bool fpga_dma_present{};
        std::string detail;
    } dma;

    struct {
        bool secure_boot_on{};
        bool setup_mode{};
        bool audit_mode{};
        bool deployed_mode{};
        int pk_bytes{};
        int kek_bytes{};
        int db_bytes{};
        int dbx_bytes{};
    } secure_boot;

    struct {
        bool txt_capable{};
        bool txt_enabled{};
        bool measured_launch{};
        int tpm_events{};
        std::string detail;
    } txt;

    struct {
        int region_count{};
        std::string detail;
    } memory;

    std::string error;
};

/// Run the full UEFI firmware analysis.
/// Scans physical memory / OS firmware APIs for RSDP, walks ACPI tables,
/// reads UEFI system table, checks DMAR/IOMMU, Secure Boot state, TXT data.
FirmwareReport analyze_firmware();

/// Demo: show all collected firmware data.
void print_report(const FirmwareReport& report);

} // namespace real::uefi::lab
