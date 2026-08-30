// uefi_phys.hpp — Shared physical / firmware-table read backend for UEFI stack.
//
// Centralizes OS access so fw/tpm/dma parsers stay pure and unit-testable
// against injected byte buffers. Live paths: Linux efivarfs/systab//dev/mem,
// Windows GetSystemFirmwareTable / EnumSystemFirmwareTables.

#pragma once

#include "real/platform.hpp"
#include "real/error.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <functional>

namespace real::uefi {

/// Sentinel physical address meaning "resolve via OS firmware APIs only"
/// (no bare-metal RSDP PA available). Used on Windows usermode.
constexpr uint64_t kOsMediatedRsdpPa = 1ull;

/// Read `size` bytes from physical address `pa` into `buf`.
/// Order: test injects → OS ACPI table cache → platform phys (/dev/mem).
bool read_physical(uint64_t pa, void* buf, size_t size);

/// Read an entire ACPI table by 4-char signature via OS services when possible.
/// On success, also registers the blob so subsequent read_physical(pa) works
/// when pa was returned by enumerate helpers.
Result<std::vector<uint8_t>> read_acpi_table_bytes(const char* signature);

/// Enumerate ACPI table signatures available through the OS.
Result<std::vector<std::string>> list_acpi_signatures();

/// Register table bytes under a synthetic physical address (for OS-mediated
/// enumeration and unit tests). Returns the assigned PA.
uint64_t cache_firmware_blob(const std::string& signature,
                             const std::vector<uint8_t>& data,
                             uint64_t preferred_pa = 0);

/// Look up a previously cached firmware blob by signature (empty if missing).
std::vector<uint8_t> cached_firmware_blob(const std::string& signature);

/// Clear OS table cache (not test injects).
void clear_firmware_cache();

// ── Test inject hooks (shipped; used by unit tests) ─────────────────

/// Install a temporary physical-read override. Pass nullptr to clear.
using PhysReadHook = std::function<bool(uint64_t pa, void* buf, size_t size)>;
void set_phys_read_hook(PhysReadHook hook);

/// Inject a contiguous physical region for the hook-less path used by tests.
void inject_phys_region(uint64_t pa, const std::vector<uint8_t>& data);
void clear_phys_injects();

/// ACPI checksum: sum of all bytes in [data, data+size) must be 0 mod 256.
bool acpi_checksum_valid(const uint8_t* data, size_t size) noexcept;

/// Compute ACPI checksum byte that makes the table valid (writes offset 9).
void acpi_fix_checksum(uint8_t* data, size_t size) noexcept;

/// Format EFI GUID (Data1-Data2-Data3-Data4) as "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX".
std::string format_efi_guid(uint32_t data1, uint16_t data2, uint16_t data3,
                            const uint8_t data4[8]);

/// Pack/unpack EFI_GUID memory layout into two uint64 halves.
/// half0 = first 8 bytes of EFI_GUID, half1 = second 8 bytes.
void efi_guid_to_halves(uint32_t data1, uint16_t data2, uint16_t data3,
                        const uint8_t data4[8],
                        uint64_t& half0, uint64_t& half1) noexcept;
void efi_guid_from_halves(uint64_t half0, uint64_t half1,
                          uint32_t& data1, uint16_t& data2, uint16_t& data3,
                          uint8_t data4[8]) noexcept;

/// Canonical string for halves (EFI Global Variable GUID etc.).
std::string efi_guid_halves_to_string(uint64_t half0, uint64_t half1);

/// Windows form "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}".
std::string efi_guid_halves_to_windows_string(uint64_t half0, uint64_t half1);

/// Parse "Name-XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX" efivar filename tail.
bool parse_efivar_filename(const std::string& filename,
                           std::string& out_name,
                           uint64_t& out_half0, uint64_t& out_half1);

// Well-known EFI Global Variable GUID halves (memory order).
// GUID: 8BE4DF61-93CA-11D2-AA0D-00E098032B8C
void efi_global_variable_guid(uint64_t& half0, uint64_t& half1) noexcept;

// Attempt to enable SE_SYSTEM_ENVIRONMENT_NAME on Windows (no-op elsewhere).
// Returns true if privilege is held or was enabled.
bool ensure_firmware_variable_privilege();

} // namespace real::uefi
