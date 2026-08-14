// uefi_var.hpp — UEFI variable enumeration and manipulation.
//
// UEFI variables control Secure Boot (PK, KEK, db, dbx), platform
// configuration, and boot options. Variable access goes through
// UEFI runtime services GetVariable/SetVariable.

#pragma once

#include "real/uefi/uefi_types.hpp"
#include "real/error.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace real::uefi {

/// A UEFI variable entry.
struct EfiVariable {
    std::string name;
    uint64_t    vendor_guid_hi;
    uint64_t    vendor_guid_lo;
    uint32_t    attributes;
    std::vector<uint8_t> data;
};

/// Enumerate all UEFI variables via GetNextVariableName.
Result<std::vector<EfiVariable>> enumerate_variables();

/// Read a specific UEFI variable by name and GUID.
Result<std::vector<uint8_t>> read_variable(const std::string& name,
    uint64_t guid_hi, uint64_t guid_lo);

/// Write a UEFI variable (requires physical presence or PK access).
Result<void> write_variable(const std::string& name,
    uint64_t guid_hi, uint64_t guid_lo,
    uint32_t attributes, const std::vector<uint8_t>& data);

/// Delete a UEFI variable.
Result<void> delete_variable(const std::string& name,
    uint64_t guid_hi, uint64_t guid_lo);

/// Check if Secure Boot variables are writable.
/// Returns true if PK/KEK/db/dbx are modifiable.
Result<bool> secure_boot_variables_writable();

/// Read the current Secure Boot state.
struct SecureBootState {
    bool secure_boot_enabled{};
    bool setup_mode{};        // TRUE = PK not enrolled
    bool audit_mode{};        // TRUE = no enforcement
    bool deployed_mode{};     // TRUE = PK enrolled
    std::vector<uint8_t> pk;
    std::vector<uint8_t> kek;
    std::vector<uint8_t> db;
    std::vector<uint8_t> dbx;
};
Result<SecureBootState> get_secure_boot_state();

/// Spoof a Secure Boot variable (requires firmware-level access).
/// From SMM or physical presence, modifies PK/KEK to accept
/// an attacker-signed module. Reports real write success/failure.
Result<void> spoof_secure_boot_variable(const std::string& name,
    const std::vector<uint8_t>& forged_data);

/// Map raw Secure Boot variable bytes into SecureBootState (shipped helper
/// used by inspection paths and unit tests — no fabricated success).
SecureBootState map_secure_boot_fields(uint8_t secure_boot, uint8_t setup_mode,
                                       uint8_t audit_mode, uint8_t deployed_mode,
                                       std::vector<uint8_t> pk,
                                       std::vector<uint8_t> kek,
                                       std::vector<uint8_t> db,
                                       std::vector<uint8_t> dbx);

} // namespace real::uefi
