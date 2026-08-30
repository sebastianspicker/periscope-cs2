// smm_interface.hpp — Real SMM (System Management Mode) research stack.
//
// Covers: SW-SMI trigger, CMOS, SMM communicate protocol, SMRAM/TSEG
// discovery, ACPI table ops, EFI runtime variables, TPM PCR ops, and
// an educational SMM physical-memory read channel.
//
// Pure protocol math lives in smm_protocol.hpp (unit-testable without
// firmware access). Platform backends perform I/O when privileges allow.

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"
#include "real/smm/smm_protocol.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace real::smm {

// ── Port I/O helpers (shared) ──────────────────────────────────────

/// Write a byte to an I/O port (Linux iopl/ioperm//dev/port, Windows lab driver).
Result<void> port_outb(std::uint16_t port, std::uint8_t value);

/// Read a byte from an I/O port.
Result<std::uint8_t> port_inb(std::uint16_t port);

// ── ACPI SMI Communication ─────────────────────────────────────────

/// Trigger an SMI via APM_CNT (port 0xB2 by default, or FADT SMI_CMD).
Result<void> trigger_smi(std::uint8_t smi_command);

/// Trigger SMI on an explicit command port (from FADT).
Result<void> trigger_smi_on_port(std::uint16_t port, std::uint8_t smi_command);

/// Write a value to an SMI CMOS/RAM index (port 0x70/0x71 path).
Result<void> smi_cmos_write(std::uint16_t index, std::uint8_t value);

/// Read a value from CMOS via index port.
Result<std::uint8_t> smi_cmos_read(std::uint16_t index);

/// Dump CMOS bank [0, length) — length capped at 256.
Result<std::vector<std::uint8_t>> cmos_dump(std::size_t length = 128);

/// Communicate with an SMM handler via a shared physical buffer.
/// Writes request, sets ready magic, triggers SW-SMI, waits for response magic.
Result<std::vector<std::uint8_t>> smm_communicate(
    std::uint64_t shared_phys_addr,
    std::size_t size,
    const std::vector<std::uint8_t>& request);

// ── SMI telemetry (blue) ───────────────────────────────────────────

/// Read MSR_SMI_COUNT (0x34). Requires /dev/cpu/*/msr or lab MSR driver.
Result<std::uint64_t> read_smi_count();

/// Measure SW-SMI round-trip latency in microseconds (best-effort).
Result<std::uint64_t> measure_smi_latency_us(std::uint8_t smi_command = 0xDE);

// ── SMRAM / TSEG ───────────────────────────────────────────────────

struct SmramInfo {
  std::uint64_t base = 0;
  std::uint64_t size = 0;
  bool locked = false;
  bool code_chk_en = false;   // SMM_Code_Chk_En observed / assumed
  bool smrr_active = false;
  std::string source;         // "tsegmb" | "smrr" | "fadt" | "unknown"
  std::string detail;
};

/// Discover SMRAM/TSEG base/size/lock state from platform sources.
Result<SmramInfo> discover_smram();

/// True if physical address appears to fall inside discovered SMRAM.
Result<bool> is_smram_address(std::uint64_t phys_addr);

// ── SMM memory channel (red research) ──────────────────────────────

/// Educational SMM physical read: packs PhysRead request, triggers SMI
/// communicate path, unpacks response. Falls back with structured error
/// when no lab handler is present.
Result<std::vector<std::uint8_t>> smm_phys_read(std::uint64_t phys_addr,
                                                 std::size_t size,
                                                 std::uint64_t comm_buffer_phys,
                                                 std::size_t comm_buffer_size);

/// Probe whether a lab SMM communicate handler answers an echo request.
Result<bool> smm_handler_present(std::uint64_t comm_buffer_phys,
                                  std::size_t comm_buffer_size);

// ── UEFI Runtime Services ──────────────────────────────────────────

/// Get the UEFI Runtime Services table pointer (physical).
Result<std::uint64_t> get_efi_runtime_services();

/// Get the UEFI System Table pointer (physical).
Result<std::uint64_t> get_efi_system_table();

/// Call UEFI SetVariable runtime service (often enters SMM).
Result<void> efi_set_variable(const char* name,
                               const std::vector<std::uint8_t>& data,
                               std::uint64_t vendor_guid_hi,
                               std::uint64_t vendor_guid_lo,
                               std::uint32_t attributes = 0x07);

/// Call UEFI GetVariable runtime service.
Result<std::vector<std::uint8_t>> efi_get_variable(
    const char* name,
    std::uint64_t vendor_guid_hi,
    std::uint64_t vendor_guid_lo);

/// Enumerate EFI variable names available to the OS (best-effort).
Result<std::vector<std::string>> efi_list_variables();

// ── ACPI tables ────────────────────────────────────────────────────

/// Locate ACPI RSDP (EFI systab, BIOS EBDA/window, or OS firmware API).
Result<std::uint64_t> find_rsdp();

/// Read an ACPI table by 4-char signature (e.g. "DSDT", "FACP", "SSDT").
Result<std::vector<std::uint8_t>> read_acpi_table(const char* signature);

/// Read the ACPI DSDT table.
Result<std::vector<std::uint8_t>> read_acpi_dsdt();

/// Override ACPI DSDT in physical memory (requires /dev/mem RW + unlocked region).
Result<void> override_acpi_dsdt(const std::vector<std::uint8_t>& new_dsdt);

/// Enumerate ACPI table signatures visible to the OS.
Result<std::vector<std::string>> list_acpi_tables();

/// Parse FADT and return SMI_CMD port info.
Result<FadtSmiInfo> read_fadt_smi_info();

// ── TPM / PCR Operations ───────────────────────────────────────────

/// Read a TPM PCR value (SHA-256 bank preferred).
Result<std::vector<std::uint8_t>> tpm_read_pcr(int pcr_index);

/// Extend a TPM PCR with a 32-byte SHA-256 digest.
Result<void> tpm_extend_pcr(int pcr_index,
                             const std::vector<std::uint8_t>& data);

/// Attempt PCR manipulation path (PCR_Event / extend). Documents SMM-class
/// spoof residual for blue attestation research.
Result<void> tpm_spoof_pcr(int pcr_index,
                            const std::vector<std::uint8_t>& fake_value);

/// Locally compute the PCR value that would result from extend (no TPM I/O).
Result<std::vector<std::uint8_t>> tpm_predict_pcr_extend(
    const std::vector<std::uint8_t>& current_pcr,
    const std::vector<std::uint8_t>& extend_digest);

}  // namespace real::smm
