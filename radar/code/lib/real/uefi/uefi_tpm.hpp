// uefi_tpm.hpp — Intel TXT and UEFI TPM configuration table access.
//
// Intel TXT (Trusted Execution Technology) provides measured launch
// and verified boot. The TPM configuration table in UEFI provides
// the TPM Event Log (TEL) for PCR measurements.

#pragma once

#include "real/uefi/uefi_types.hpp"
#include "real/error.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace real::uefi {

/// TPM Event Log entry from UEFI configuration table / ACPI TPM2.
struct TpmEventLogEntry {
    uint32_t pcr_index{};
    uint32_t event_type{};
    std::vector<uint8_t> digest;       // first bank digest (if any)
    std::vector<uint8_t> event_data;
    std::string description;
    uint32_t digest_count{};
};

/// Read the TPM Event Log from the UEFI configuration table or OS sources.
Result<std::vector<TpmEventLogEntry>> read_tpm_event_log();

/// Pure parser for TCG EFI Spec ID Event + TCG_PCR_EVENT2 stream.
/// Also accepts legacy TCG_PCR_EVENT (SHA-1) records.
Result<std::vector<TpmEventLogEntry>> parse_tpm_event_log(const uint8_t* data,
                                                          size_t size);

/// Read the Intel TXT heap / public space status.
struct IntelTxtData {
    bool    txt_capable{};
    bool    txt_enabled{};
    bool    smx_enabled{};
    bool    measured_launch_occurred{};
    uint64_t heap_base{};
    uint64_t heap_size{};
    uint32_t bios_spec_ver{};
    uint32_t acpi_data_count{};
    std::vector<uint8_t> mle_pcr_value;   // PCR 17-18 values when present
    std::string detail;
};
Result<IntelTxtData> read_intel_txt_data();

/// Pure parse of a TXT heap blob (size/reserved/entry_count + entries).
Result<IntelTxtData> parse_txt_heap(const uint8_t* data, size_t size);

/// Check if the platform supports TXT (SMX + VMX + TPM).
Result<bool> txt_platform_supported();

/// Check if a measured launch occurred during this boot.
Result<bool> measured_launch_occurred();

/// TPM algorithm id → digest size (TCG algorithm registry).
size_t tpm_alg_digest_size(uint16_t alg_id) noexcept;

} // namespace real::uefi
