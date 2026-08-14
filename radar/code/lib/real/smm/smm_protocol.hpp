// smm_protocol.hpp — Pure SMM/ACPI/TPM protocol helpers (no OS I/O).
//
// Unit-testable against synthetic buffers. Platform backends in
// smi/acpi/tpm/smram/smm_channel supply physical access and port I/O.
//
// References:
//   Intel SDM Vol 3C Ch. 34 (SMM), ACPI 6.4, UEFI PI SMM CIS,
//   TCG TPM 2.0 Library Spec, Intel chipset EDS (TSEGMB).

#pragma once

#include "real/error.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace real::smm {

// ── Hardware constants ─────────────────────────────────────────────

/// APM / SMI command port (legacy ACPI PM).
constexpr std::uint16_t kSmiCommandPort = 0xB2;
/// APM status port (read-back on many chipsets).
constexpr std::uint16_t kSmiStatusPort = 0xB3;
/// CMOS index / data ports.
constexpr std::uint16_t kCmosIndexPort = 0x70;
constexpr std::uint16_t kCmosDataPort = 0x71;
/// RTC/CMOS NMI-mask bit in index port.
constexpr std::uint8_t kCmosNmiDisable = 0x80;

/// MSR_SMI_COUNT — increments on every SMI (Intel, read-only).
constexpr std::uint32_t kMsrSmiCount = 0x34;
/// IA32_SMRR_PHYSBASE / PHYSMASK (when SMRR supported).
constexpr std::uint32_t kMsrSmrrPhysBase = 0x1F2;
constexpr std::uint32_t kMsrSmrrPhysMask = 0x1F3;
/// MSR_SMM_FEATURE_CONTROL / SMM_MCA_CAP related (feature bits).
constexpr std::uint32_t kMsrSmmFeatureControl = 0x4E0;
/// SMM_Code_Chk_En lives in MSR_SMM_FEATURE_CONTROL bit 2 on many CPUs.
constexpr std::uint64_t kSmmCodeChkEnBit = 1ull << 2;

/// Intel Host Bridge PCI config: TSEG Memory Base (TSEGMB) — B0:D0:F0 + 0xB8.
constexpr std::uint8_t kIntelTsegmbOffset = 0xB8;
/// TSEGMB lock bit (bit 0 = lock on many 100-series+).
constexpr std::uint32_t kTsegmbLockBit = 0x1u;
/// TSEG base address mask (bits 31:20 or wider depending on platform).
constexpr std::uint32_t kTsegmbBaseMask = 0xFFF00000u;

/// SMM communicate magic written into shared buffer by OS / SMM handler.
constexpr std::uint32_t kSmmReqMagic = 0x534D4D01u;  // "SMM\1"
constexpr std::uint32_t kSmmRspMagic = 0x534D4D02u;  // "SMM\2"
constexpr std::uint32_t kSmmErrMagic = 0x534D4D03u;  // "SMM\3"

/// Lab SMM handler SW-SMI command codes (educational catalog).
enum class SmiCommand : std::uint8_t {
  Nop = 0x00,
  Communicate = 0x01,
  PhysRead = 0x10,
  PhysWrite = 0x11,
  GetSmramInfo = 0x20,
  CmosSync = 0x30,
  TpmAssist = 0x40,
  Echo = 0xDE,  // matches dma acpi_smi_read probe
};

// ── ACPI pure helpers ──────────────────────────────────────────────

/// ACPI checksum: sum of all bytes in [data, data+len) mod 256 == 0.
bool acpi_checksum_valid(const std::uint8_t* data, std::size_t len);

/// Compute the single-byte value that makes the table checksum to 0.
/// `checksum_offset` is the byte index of the checksum field (9 for SDT header).
std::uint8_t acpi_fix_checksum(std::vector<std::uint8_t>& table,
                                 std::size_t checksum_offset = 9);

/// Validate RSDP signature "RSD PTR " and checksums (rev1 + rev2 extended).
bool rsdp_valid(const std::uint8_t* rsdp, std::size_t len);

/// Extract RSDT (32-bit) and optional XSDT (64-bit) addresses from RSDP bytes.
struct RsdpAddresses {
  std::uint8_t revision = 0;
  std::uint32_t rsdt = 0;
  std::uint64_t xsdt = 0;
  bool ok = false;
};
RsdpAddresses parse_rsdp_addresses(const std::uint8_t* rsdp, std::size_t len);

/// FADT (FACP) fields needed for SMI: SMI_CMD port + enable/disable values.
struct FadtSmiInfo {
  std::uint32_t smi_cmd = 0;       // I/O port (often 0xB2)
  std::uint8_t acpi_enable = 0;
  std::uint8_t acpi_disable = 0;
  std::uint8_t s4bios_req = 0;
  std::uint8_t pstate_cnt = 0;
  bool ok = false;
};
/// Parse FADT bytes (signature FACP). Requires length >= 54 (ACPI 1.0 FADT).
FadtSmiInfo parse_fadt_smi(const std::uint8_t* fadt, std::size_t len);

// ── TSEG / SMRAM pure helpers ──────────────────────────────────────

struct TsegDecode {
  std::uint64_t base = 0;
  std::uint64_t size = 0;   // 0 if size unknown from TSEGMB alone
  bool locked = false;
  bool valid = false;
};

/// Decode Intel TSEGMB register dword into base + lock bit.
/// `tseg_size_hint` (bytes) is used when size is known from TOLUD/BGSM delta
/// or platform docs; 0 leaves size=0.
TsegDecode decode_tsegmb(std::uint32_t tsegmb, std::uint64_t tseg_size_hint = 0);

/// True if a physical address falls inside [base, base+size).
bool phys_in_range(std::uint64_t phys, std::uint64_t base, std::uint64_t size);

// ── SMM communicate protocol (lab + UEFI-style header) ─────────────

/// Compact lab request placed at the start of a shared physical buffer.
/// Layout (little-endian):
///   [0..3]   magic (kSmmReqMagic)
///   [4]      command (SmiCommand)
///   [5..7]   reserved
///   [8..15]  phys_addr
///   [16..19] size
///   [20..]   payload (optional)
#pragma pack(push, 1)
struct SmmCommHeader {
  std::uint32_t magic = 0;
  std::uint8_t command = 0;
  std::uint8_t reserved[3]{};
  std::uint64_t phys_addr = 0;
  std::uint32_t size = 0;
};
#pragma pack(pop)

static_assert(sizeof(SmmCommHeader) == 20, "SmmCommHeader size");

/// UEFI PI EFI_SMM_COMMUNICATE_HEADER (GUID + MessageLength + Data).
#pragma pack(push, 1)
struct EfiSmmCommunicateHeader {
  std::uint8_t header_guid[16]{};
  std::uint64_t message_length = 0;
  // uint8_t data[] follows
};
#pragma pack(pop)

/// Pack a phys-read request into a byte buffer suitable for smm_communicate.
std::vector<std::uint8_t> pack_phys_read_request(std::uint64_t phys_addr,
                                                  std::uint32_t size);

/// Pack an echo/nop request (used to probe handler presence).
std::vector<std::uint8_t> pack_echo_request(std::uint8_t token);

/// Parse response buffer; on success returns payload after header.
/// Expects magic kSmmRspMagic; fails on kSmmErrMagic or unknown.
Result<std::vector<std::uint8_t>> unpack_smm_response(
    const std::vector<std::uint8_t>& buffer);

/// Build a full UEFI-style communicate buffer: EFI header + lab SmmCommHeader.
std::vector<std::uint8_t> pack_efi_smm_communicate(
    const std::uint8_t handler_guid[16],
    const std::vector<std::uint8_t>& message);

// ── CMOS pure helpers ──────────────────────────────────────────────

/// Standard CMOS checksum over bytes [start, end] inclusive (often 0x10-0x2D).
std::uint16_t cmos_checksum(const std::uint8_t* cmos, std::size_t start,
                            std::size_t end);

/// Verify CMOS checksum stored at (high, low) indices against range.
bool cmos_checksum_valid(const std::uint8_t* cmos, std::size_t len,
                         std::size_t range_start, std::size_t range_end,
                         std::size_t stored_hi, std::size_t stored_lo);

// ── TPM PCR extend math ────────────────────────────────────────────

/// Compact SHA-256 (FIPS 180-4). Pure host implementation for PCR model.
std::array<std::uint8_t, 32> sha256(const std::uint8_t* data, std::size_t len);
std::array<std::uint8_t, 32> sha256(const std::vector<std::uint8_t>& data);

/// TPM2 PCR extend model: new = SHA256(old || digest).
/// Both `old_pcr` and `extend_digest` must be 32 bytes (SHA-256 bank).
Result<std::array<std::uint8_t, 32>> pcr_extend_sha256(
    const std::array<std::uint8_t, 32>& old_pcr,
    const std::array<std::uint8_t, 32>& extend_digest);

/// Same with vectors (must be size 32).
Result<std::vector<std::uint8_t>> pcr_extend_sha256(
    const std::vector<std::uint8_t>& old_pcr,
    const std::vector<std::uint8_t>& extend_digest);

// ── SW-SMI latency heuristic (blue) ────────────────────────────────

/// Classify SMI latency sample (microseconds) as normal / elevated / anomalous.
enum class SmiLatencyClass { Normal, Elevated, Anomalous };

/// Heuristic thresholds used by blue SMI profiling (educational defaults).
SmiLatencyClass classify_smi_latency_us(std::uint64_t latency_us);

/// Expected delta of MSR_SMI_COUNT for N deliberate SW-SMI triggers.
/// Returns true if observed_delta matches expected (within slack).
bool smi_count_delta_plausible(std::uint64_t before, std::uint64_t after,
                               std::uint64_t expected_triggers,
                               std::uint64_t slack = 2);

}  // namespace real::smm
