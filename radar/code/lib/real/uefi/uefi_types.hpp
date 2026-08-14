// uefi_types.hpp — UEFI firmware structures and type definitions.
//
// Covers UEFI 2.9+ firmware table enumeration, ACPI+UEFI config tables,
// runtime/boot services pointers, memory map, and Intel TXT.
//
// Reference: UEFI Specification 2.9, Intel TXT Specification

#pragma once

#include "real/platform.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace real::uefi {

// ═══════════════════════════════════════════════════════════════════════
// ACPI RSDP / RSDT / XSDT
// ═══════════════════════════════════════════════════════════════════════

struct alignas(8) AcpiRsdp {
    char     signature[8]{};     // "RSD PTR "
    uint8_t  checksum{};
    char     oem_id[6]{};
    uint8_t  revision{};
    uint32_t rsdt_address{};     // 32-bit RSDT address
    uint32_t length{};           // RSDP length (rev >= 2)
    uint64_t xsdt_address{};     // 64-bit XSDT address (rev >= 2)
    uint8_t  extended_checksum{};
    uint8_t  reserved[3]{};

    bool valid() const noexcept;
};

struct alignas(4) AcpiSdtHeader {
    char     signature[4]{};
    uint32_t length{};
    uint8_t  revision{};
    uint8_t  checksum{};
    char     oem_id[6]{};
    char     oem_table_id[8]{};
    uint32_t oem_revision{};
    uint32_t creator_id{};
    uint32_t creator_revision{};
};

// ═══════════════════════════════════════════════════════════════════════
// UEFI System Table
// ═══════════════════════════════════════════════════════════════════════

struct alignas(8) EfiTableHeader {
    uint64_t signature{};
    uint32_t revision{};
    uint32_t header_size{};
    uint32_t crc32{};
    uint32_t reserved{};
};

struct alignas(8) EfiRuntimeServicesTable {
    EfiTableHeader header;
    // 14 function pointers follow in UEFI spec order
    uint64_t get_time;
    uint64_t set_time;
    uint64_t get_wakeup_time;
    uint64_t set_wakeup_time;
    uint64_t set_virtual_address_map;
    uint64_t convert_pointer;
    uint64_t get_variable;
    uint64_t get_next_variable_name;
    uint64_t set_variable;
    uint64_t get_next_high_monotonic_count;
    uint64_t reset_system;
    uint64_t update_capsule;
    uint64_t query_capsule_capabilities;
    uint64_t query_variable_info;
};

// EFI_GUID memory layout as two little-endian uint64 halves:
//   vendor_guid_hi = first 8 bytes (Data1|Data2|Data3 LE)
//   vendor_guid_lo = second 8 bytes (Data4[8])
struct alignas(8) EfiConfigurationTable {
    uint64_t vendor_guid_hi;
    uint64_t vendor_guid_lo;
    uint64_t vendor_table;
};

// UEFI System Table (x64 layout: pointers are 8 bytes, handles 8 bytes)
struct alignas(8) EfiSystemTable {
    EfiTableHeader           header;
    uint64_t                 firmware_vendor;   // CHAR16*
    uint32_t                 firmware_revision;
    uint32_t                 _pad0{};
    uint64_t                 console_in_handle;
    uint64_t                 con_in;
    uint64_t                 console_out_handle;
    uint64_t                 con_out;
    uint64_t                 console_err_handle;
    uint64_t                 con_err;
    uint64_t                 runtime_services;
    uint64_t                 boot_services;
    uint64_t                 number_of_table_entries;
    uint64_t                 configuration_table;
};

// ═══════════════════════════════════════════════════════════════════════
// UEFI Memory Map
// ═══════════════════════════════════════════════════════════════════════

enum class EfiMemoryType : uint32_t {
    Reserved = 0,
    LoaderCode,
    LoaderData,
    BootServicesCode,
    BootServicesData,
    RuntimeServicesCode,
    RuntimeServicesData,
    ConventionalMemory,
    UnusableMemory,
    AcpiReclaimMemory,
    AcpiMemoryNvs,
    MemoryMappedIO,
    MemoryMappedIOPortSpace,
    PalCode,
    PersistentMemory,
    UnacceptedMemoryType,
};

struct alignas(8) EfiMemoryDescriptor {
    uint32_t type;              // EfiMemoryType
    uint32_t padding;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;         // EFI_MEMORY_XX flags
};

// ═══════════════════════════════════════════════════════════════════════
// UEFI GUID constants (EFI_GUID halves: half0 first 8B, half1 second 8B)
// ═══════════════════════════════════════════════════════════════════════

namespace guid {
    // 8BE4DF61-93CA-11D2-AA0D-00E098032B8C  EFI Global Variable
    constexpr uint64_t EfiGlobalVariable0 = 0x11D293CA8BE4DF61ULL;
    constexpr uint64_t EfiGlobalVariable1 = 0x8C2B0398E0000DAAULL;
    // Aliases matching prior hi/lo naming (half0 / half1)
    constexpr uint64_t EfiGlobalVariableHi = EfiGlobalVariable0;
    constexpr uint64_t EfiGlobalVariable   = EfiGlobalVariable1;

    // EB9D2D31-2D88-11D3-9A16-0090273FC14D  ACPI 1.0 RSDP config (legacy)
    // EB9D2D30-2D88-11D3-9A16-0090273FC14D  ACPI table
    // Not all GUIDs are needed for table walks; DMAR is an ACPI table.

    // 8868E871-E4F1-11D3-BC22-0080C73C8881  ACPI 2.0
    constexpr uint64_t EfiAcpi20Table0 = 0x11D3E4F18868E871ULL;
    constexpr uint64_t EfiAcpi20Table1 = 0x81883CC7800022BCULL;

    // 6C34322E-BE24-4E95-3CEC-20EB (partial historical) — TCG2 / TPM2
    // Canonical TCG2 final events / EFI_TCG2_PROTOCOL not needed for ACPI TPM2.

    // Placeholder TPM config table halves (matched when present in systab walk)
    constexpr uint64_t EfiTpmTableHi = 0x6C34322EBE244E95ULL;
    constexpr uint64_t EfiTpmTableLo = 0x3CEC20EBULL;
    constexpr uint64_t EfiTpmTable0  = EfiTpmTableHi;
    constexpr uint64_t EfiTpmTable1  = EfiTpmTableLo;

    constexpr uint64_t EfiDmarTable       = 0xA361428C4E5D4271ULL;
    constexpr uint64_t EfiDmarTableHi     = 0xC684A581ULL;
    constexpr uint64_t EfiMemoryAttribute = 0xCC1C5E6B2E38A5D8ULL;
    constexpr uint64_t EfiMemoryAttributeHi = 0xD60B787DULL;
}

// ═══════════════════════════════════════════════════════════════════════
// Intel TXT / TPM
// ═══════════════════════════════════════════════════════════════════════

struct alignas(8) IntelTxtHeap {
    uint64_t    size;
    uint64_t    reserved;
    uint64_t    entry_count;
    // TXT.HEAP entries follow
};

struct alignas(8) IntelTxtHeapEntry {
    uint32_t type;
    uint32_t padding;
    uint64_t size;
    // Entry-specific data follows
};

// TXT heap entry types
constexpr uint32_t kTxtHeapBiosSpecVer  = 1;
constexpr uint32_t kTxtHeapAcpiData    = 2;
constexpr uint32_t kTxtHeapE820Entry   = 4;

// Public TXT register space (Intel TXT Software Development Guide)
constexpr uint64_t kTxtPublicBase = 0xFED30000ull;
constexpr uint32_t kTxtStsOffset  = 0x000;   // TXT.STS
constexpr uint32_t kTxtHeapBaseOff = 0x300;  // TXT.HEAP.BASE (public space layout varies;
                                             // heap base commonly at +0x300 in public)
constexpr uint32_t kTxtHeapSizeOff = 0x308;

// ═══════════════════════════════════════════════════════════════════════
// DMA Remapping Tables (DMAR / IVRS)
// ═══════════════════════════════════════════════════════════════════════

struct alignas(4) AcpiDmarHeader {
    AcpiSdtHeader header;
    uint8_t  host_address_width;
    uint8_t  flags;
    uint8_t  reserved[10];
};

// DMAR remapping structure types
constexpr uint16_t kDmarTypeDrhd = 0;
constexpr uint16_t kDmarTypeRmrr = 1;
constexpr uint16_t kDmarTypeAtsr = 2;
constexpr uint16_t kDmarTypeRhsa = 3;
constexpr uint16_t kDmarTypeAndd = 4;
constexpr uint16_t kDmarTypeSatc = 5;

#pragma pack(push, 1)
struct AcpiDmarDrhd {
    uint16_t type;          // 0 = DRHD
    uint16_t length;
    uint8_t  flags;         // bit0 INCLUDE_PCI_ALL
    uint8_t  reserved;
    uint16_t segment;
    uint64_t base_address;  // Register base for this remap unit
    // Device scope entries follow (variable)
};
#pragma pack(pop)
static_assert(sizeof(AcpiDmarDrhd) == 16, "DRHD fixed fields must be 16 bytes");

// AMD IVRS header after AcpiSdtHeader (48 bytes on wire)
#pragma pack(push, 1)
struct AcpiIvrsHeader {
    AcpiSdtHeader header;
    uint32_t iv_info;
    uint64_t reserved;
};
#pragma pack(pop)
static_assert(sizeof(AcpiIvrsHeader) == 48, "IVRS header must be 48 bytes");

// IVHD type 0x10 / 0x11
constexpr uint8_t kIvhdType10 = 0x10;
constexpr uint8_t kIvhdType11 = 0x11;
constexpr uint8_t kIvmdType20 = 0x20;
constexpr uint8_t kIvmdType21 = 0x21;
constexpr uint8_t kIvmdType22 = 0x22;

// MCFG configuration space allocation structure (16 bytes on wire)
#pragma pack(push, 1)
struct AcpiMcfgAllocation {
    uint64_t base_address;
    uint16_t segment_group;
    uint8_t  start_bus;
    uint8_t  end_bus;
    uint32_t reserved;
};
#pragma pack(pop)
static_assert(sizeof(AcpiMcfgAllocation) == 16, "MCFG allocation must be 16 bytes");

// EFI System Table signature "IBI SYST" as little-endian uint64
constexpr uint64_t kEfiSystemTableSignature = 0x5453595320494249ULL;

} // namespace real::uefi
