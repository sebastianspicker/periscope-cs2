// uefi_fw.cpp — UEFI firmware table scanning and ACPI walk (runtime I/O).

#include "real/uefi/uefi_fw.hpp"
#include "real/uefi/uefi_phys.hpp"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <sstream>

#if LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <unistd.h>
#endif

namespace real::uefi {
namespace {

std::vector<uint8_t> read_file_bytes(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return {};
    if (std::fseek(f, 0, SEEK_END) != 0) { std::fclose(f); return {}; }
    long sz = std::ftell(f);
    if (sz <= 0) { std::fclose(f); return {}; }
    std::rewind(f);
    std::vector<uint8_t> out(static_cast<size_t>(sz));
    size_t n = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    out.resize(n);
    return out;
}

std::string read_file_text(const std::string& path) {
    auto b = read_file_bytes(path);
    return std::string(reinterpret_cast<const char*>(b.data()), b.size());
}

} // anonymous namespace
Result<uint64_t> find_rsdp_pa() {
#if LR_PLATFORM_LINUX
    // Prefer EFI systab (no /dev/mem required)
    {
        auto text = read_file_text("/sys/firmware/efi/systab");
        if (!text.empty()) {
            auto pos = text.find("ACPI20=");
            if (pos == std::string::npos) pos = text.find("ACPI=");
            if (pos != std::string::npos) {
                uint64_t address = 0;
                const char* p = text.c_str() + pos;
                if (std::sscanf(p, "ACPI20=0x%llx",
                        reinterpret_cast<unsigned long long*>(&address)) == 1 ||
                    std::sscanf(p, "ACPI=0x%llx",
                        reinterpret_cast<unsigned long long*>(&address)) == 1) {
                    if (address) {
                        // Validate if readable
                        uint8_t buf[36]{};
                        if (read_physical(address, buf, sizeof(buf))) {
                            auto parsed = parse_rsdp(buf, sizeof(buf));
                            if (parsed) {
                                std::printf("[uefi] RSDP from EFI systab: 0x%llx rev=%u\n",
                                    static_cast<unsigned long long>(address),
                                    parsed->revision);
                                return Result<uint64_t>(address);
                            }
                        } else {
                            // Cache as OS-mediated marker but keep reported PA
                            std::printf("[uefi] RSDP PA from systab 0x%llx (phys read denied)\n",
                                static_cast<unsigned long long>(address));
                            return Result<uint64_t>(address);
                        }
                    }
                }
            }
        }
    }
#endif

    // Physical scan EBDA + BIOS ROM window
    const uint64_t scan_regions[][2] = {
        {0x80000, 0x20000},   // EBDA area
        {0xE0000, 0x20000},   // BIOS area
    };
    uint8_t buf[0x1000];
    for (auto r : scan_regions) {
        for (uint64_t off = 0; off < r[1]; off += 0x1000) {
            if (!read_physical(r[0] + off, buf, sizeof(buf))) continue;
            for (size_t i = 0; i + 20 <= sizeof(buf); i += 16) {
                if (std::memcmp(buf + i, "RSD PTR ", 8) != 0) continue;
                auto parsed = parse_rsdp(buf + i, sizeof(buf) - i);
                if (!parsed) continue;
                const uint64_t rsdp_pa = r[0] + off + i;
                std::printf("[uefi] RSDP found at PA 0x%llx (rev %u)\n",
                            static_cast<unsigned long long>(rsdp_pa),
                            parsed->revision);
                return Result<uint64_t>(rsdp_pa);
            }
        }
    }

#if LR_PLATFORM_WINDOWS
    // Windows usermode: no RSDP PA — mediate via firmware table APIs.
    auto sigs = list_acpi_signatures();
    if (sigs && !sigs->empty()) {
        std::printf("[uefi] RSDP PA unavailable; using OS-mediated ACPI (%zu tables)\n",
                    sigs->size());
        return Result<uint64_t>(kOsMediatedRsdpPa);
    }
    return Result<uint64_t>(0, "RSDP not found (Windows firmware tables unavailable)");
#else
    return Result<uint64_t>(0, "RSDP not found in physical memory scan");
#endif
}

Result<AcpiRsdp> read_rsdp(uint64_t rsdp_pa) {
    if (rsdp_pa == kOsMediatedRsdpPa) {
        // Synthetic RSDP indicating OS-mediated path (checksums valid).
        AcpiRsdp rsdp{};
        std::memcpy(rsdp.signature, "RSD PTR ", 8);
        rsdp.revision = 2;
        rsdp.length = 36;
        rsdp.xsdt_address = 0;
        rsdp.rsdt_address = 0;
        uint8_t* b = reinterpret_cast<uint8_t*>(&rsdp);
        // Byte 8 = checksum over first 20 bytes
        b[8] = 0;
        {
            uint8_t sum = 0;
            for (int i = 0; i < 20; ++i) sum = static_cast<uint8_t>(sum + b[i]);
            b[8] = static_cast<uint8_t>((256 - sum) & 0xFF);
        }
        // Byte 32 = extended_checksum over full 36-byte RSDP
        b[32] = 0;
        {
            uint8_t sum = 0;
            for (int i = 0; i < 36; ++i) sum = static_cast<uint8_t>(sum + b[i]);
            b[32] = static_cast<uint8_t>((256 - sum) & 0xFF);
        }
        return Result<AcpiRsdp>(rsdp);
    }
    uint8_t buf[sizeof(AcpiRsdp)]{};
    if (!read_physical(rsdp_pa, buf, sizeof(buf)))
        return Result<AcpiRsdp>({}, "Failed to read RSDP");
    return parse_rsdp(buf, sizeof(buf));
}

// ═══════════════════════════════════════════════════════════════════════
// ACPI table enumeration
// ═══════════════════════════════════════════════════════════════════════

Result<std::vector<AcpiTableEntry>> enumerate_acpi_tables(uint64_t rsdp_pa) {
    // OS-mediated or Windows path: use firmware table list
    if (rsdp_pa == kOsMediatedRsdpPa) {
        auto sigs = list_acpi_signatures();
        if (!sigs) return Result<std::vector<AcpiTableEntry>>({}, sigs.error_msg);
        std::vector<AcpiTableEntry> tables;
        // Deduplicate identical signatures for the OS-mediated path: Windows
        // EnumSystemFirmwareTables may list SSDT many times while
        // GetSystemFirmwareTable returns a single instance.
        std::vector<std::string> unique;
        for (const auto& sig : *sigs) {
            const std::string key = sig.substr(0, 4);
            if (std::find(unique.begin(), unique.end(), key) == unique.end())
                unique.push_back(key);
        }
        for (const auto& sig : unique) {
            auto bytes = read_acpi_table_bytes(sig.c_str());
            if (!bytes) continue;
            uint64_t pa = cache_firmware_blob(sig, *bytes);
            AcpiTableEntry e;
            e.signature = sig.substr(0, 4);
            e.phys_addr = pa;
            e.size = static_cast<uint32_t>(bytes->size());
            tables.push_back(std::move(e));
        }
        if (tables.empty())
            return Result<std::vector<AcpiTableEntry>>({}, "No ACPI tables from OS firmware APIs");
        std::printf("[uefi] Enumerated %zu ACPI tables via OS firmware APIs\n", tables.size());
        return Result<std::vector<AcpiTableEntry>>(std::move(tables));
    }

#if LR_PLATFORM_LINUX
    // Prefer sysfs listing when available (works without /dev/mem)
    {
        auto sigs = list_acpi_signatures();
        if (sigs && !sigs->empty()) {
            std::vector<AcpiTableEntry> tables;
            for (const auto& sig : *sigs) {
                auto bytes = read_acpi_table_bytes(sig.c_str());
                if (!bytes) continue;
                uint64_t pa = cache_firmware_blob(sig, *bytes);
                AcpiTableEntry e;
                e.signature = sig.substr(0, 4);
                e.phys_addr = pa;
                e.size = static_cast<uint32_t>(bytes->size());
                tables.push_back(std::move(e));
            }
            if (!tables.empty()) {
                std::printf("[uefi] Enumerated %zu ACPI tables from sysfs\n", tables.size());
                return Result<std::vector<AcpiTableEntry>>(std::move(tables));
            }
        }
    }
#endif

    auto rsdp_result = read_rsdp(rsdp_pa);
    if (!rsdp_result)
        return Result<std::vector<AcpiTableEntry>>({}, rsdp_result.error_msg);

    auto& rsdp = *rsdp_result;
    const bool use_xsdt = (rsdp.revision >= 2 && rsdp.xsdt_address != 0);
    uint64_t sdt_pa = use_xsdt ? rsdp.xsdt_address : rsdp.rsdt_address;
    if (!sdt_pa)
        return Result<std::vector<AcpiTableEntry>>({}, "No RSDT/XSDT address in RSDP");

    AcpiSdtHeader sdt_header{};
    if (!read_physical(sdt_pa, &sdt_header, sizeof(sdt_header)))
        return Result<std::vector<AcpiTableEntry>>({}, "Failed to read RSDT/XSDT header");
    if (sdt_header.length < sizeof(AcpiSdtHeader) || sdt_header.length > 0x100000)
        return Result<std::vector<AcpiTableEntry>>({}, "RSDT/XSDT length invalid");

    std::vector<uint8_t> sdt(sdt_header.length);
    if (!read_physical(sdt_pa, sdt.data(), sdt.size()))
        return Result<std::vector<AcpiTableEntry>>({}, "Failed to read RSDT/XSDT body");

    auto addrs = parse_sdt_entry_addresses(sdt.data(), sdt.size(), use_xsdt);
    if (!addrs) return Result<std::vector<AcpiTableEntry>>({}, addrs.error_msg);

    std::vector<AcpiTableEntry> tables;
    for (uint64_t table_pa : *addrs) {
        AcpiSdtHeader hdr{};
        if (!read_physical(table_pa, &hdr, sizeof(hdr))) continue;
        if (hdr.length < sizeof(AcpiSdtHeader) || hdr.length > 0x1000000) continue;
        std::vector<uint8_t> body(hdr.length);
        if (read_physical(table_pa, body.data(), body.size())) {
            std::string sig(hdr.signature, 4);
            cache_firmware_blob(sig, body, table_pa);
        }
        AcpiTableEntry entry;
        entry.signature = std::string(hdr.signature, 4);
        entry.phys_addr = table_pa;
        entry.size = hdr.length;
        tables.push_back(entry);
    }

    std::printf("[uefi] Enumerated %zu ACPI tables from XSDT/RSDT\n", tables.size());
    return Result<std::vector<AcpiTableEntry>>(std::move(tables));
}

Result<AcpiTableEntry> find_acpi_table(const std::string& signature) {
    if (signature.size() < 4)
        return Result<AcpiTableEntry>({}, "ACPI signature must be 4 characters");
    const std::string sig = signature.substr(0, 4);

    // Fast path: OS firmware table
    auto bytes = read_acpi_table_bytes(sig.c_str());
    if (bytes && !bytes->empty()) {
        uint64_t pa = cache_firmware_blob(sig, *bytes);
        AcpiTableEntry e{sig, pa, static_cast<uint32_t>(bytes->size())};
        return Result<AcpiTableEntry>(e);
    }

    auto rsdp = find_rsdp_pa();
    if (!rsdp) return Result<AcpiTableEntry>({}, rsdp.error_msg);
    auto tables = enumerate_acpi_tables(*rsdp);
    if (!tables) return Result<AcpiTableEntry>({}, tables.error_msg);
    for (auto& t : *tables) {
        if (t.signature == sig) return Result<AcpiTableEntry>(t);
    }
    return Result<AcpiTableEntry>({}, "ACPI table " + sig + " not found");
}

// ═══════════════════════════════════════════════════════════════════════
// UEFI System Table
// ═══════════════════════════════════════════════════════════════════════

Result<EfiSystemTable> read_efi_system_table(uint64_t /*rsdp_pa*/) {
#if LR_PLATFORM_LINUX
    {
        auto text = read_file_text("/sys/firmware/efi/systab");
        auto pos = text.find("EFI_SYSTEM_TABLE=");
        if (pos != std::string::npos) {
            uint64_t address = 0;
            if (std::sscanf(text.c_str() + pos, "EFI_SYSTEM_TABLE=0x%llx",
                    reinterpret_cast<unsigned long long*>(&address)) == 1 && address) {
                uint8_t buf[sizeof(EfiSystemTable)]{};
                if (read_physical(address, buf, sizeof(buf))) {
                    auto st = parse_efi_system_table(buf, sizeof(buf));
                    if (st) {
                        std::printf("[uefi] EFI System Table at PA 0x%llx (rev 0x%x)\n",
                                    static_cast<unsigned long long>(address),
                                    st->header.revision);
                        return st;
                    }
                }
                return Result<EfiSystemTable>({},
                    "EFI_SYSTEM_TABLE PA known but physical read failed");
            }
        }
    }
#endif

    // Scan common ranges for "IBI SYST" signature
    const uint64_t scan_ranges[][2] = {
        {0x100000, 0xA00000},
        {0x7F000000, 0x1000000},
    };
    for (auto r : scan_ranges) {
        uint8_t buf[4096];
        for (uint64_t off = 0; off < r[1]; off += 0x1000) {
            if (!read_physical(r[0] + off, buf, sizeof(buf))) continue;
            for (size_t i = 0; i + sizeof(EfiTableHeader) <= sizeof(buf); i += 8) {
                uint64_t sig = 0;
                std::memcpy(&sig, buf + i, 8);
                if (sig != kEfiSystemTableSignature) continue;
                // Header is at start of EfiSystemTable
                auto st = parse_efi_system_table(buf + i, sizeof(buf) - i);
                if (!st) continue;
                std::printf("[uefi] EFI System Table at PA 0x%llx (rev 0x%x)\n",
                            static_cast<unsigned long long>(r[0] + off + i),
                            st->header.revision);
                return st;
            }
        }
    }
    return Result<EfiSystemTable>({}, "EFI System Table not found");
}

Result<std::vector<ConfigTableEntry>> enumerate_config_table(
    const EfiSystemTable& st) {
    if (!st.configuration_table || !st.number_of_table_entries)
        return Result<std::vector<ConfigTableEntry>>(
            {}, "No configuration table in system table");

    if (st.number_of_table_entries > 512)
        return Result<std::vector<ConfigTableEntry>>(
            {}, "Implausible configuration table entry count");

    size_t entries = static_cast<size_t>(st.number_of_table_entries);
    size_t buf_size = entries * sizeof(EfiConfigurationTable);
    std::vector<uint8_t> buf(buf_size);
    if (!read_physical(st.configuration_table, buf.data(), buf_size))
        return Result<std::vector<ConfigTableEntry>>(
            {}, "Failed to read configuration table");
    return parse_config_table_array(buf.data(), buf.size(), entries);
}

Result<uint64_t> find_config_table(const EfiSystemTable& st,
                                    uint64_t guid_hi, uint64_t guid_lo) {
    auto ct = enumerate_config_table(st);
    if (!ct) return Result<uint64_t>(0, ct.error_msg);
    for (auto& entry : *ct) {
        if (entry.vendor_guid_hi == guid_hi && entry.vendor_guid_lo == guid_lo)
            return Result<uint64_t>(entry.table_pa);
    }
    return Result<uint64_t>(0, "Configuration table GUID not found");
}

// ═══════════════════════════════════════════════════════════════════════
// DMA Remapping (DMAR / IVRS)
// ═══════════════════════════════════════════════════════════════════════

Result<DmarInfo> read_dmar_table() {
    auto table = find_acpi_table("DMAR");
    if (!table)
        return Result<DmarInfo>({}, std::string("DMAR table not found: ") + table.error_msg.c_str());

    std::vector<uint8_t> body = cached_firmware_blob("DMAR");
    if (body.empty()) {
        body.resize(table->size);
        if (!read_physical(table->phys_addr, body.data(), body.size()))
            return Result<DmarInfo>({}, "Failed to read DMAR table body");
    }
    auto info = parse_dmar_table(body.data(), body.size());
    if (!info) return info;
    info->detail += " pa=0x" + std::to_string(table->phys_addr);
    std::printf("[uefi] DMAR: %d DRHD units, remap=%s, addr_width=%d\n",
                info->drhd_count, info->dma_remap_active ? "active" : "absent",
                info->host_address_width);
    return info;
}

Result<DmarInfo> read_ivrs_table() {
    auto table = find_acpi_table("IVRS");
    if (!table) return Result<DmarInfo>({}, "IVRS table not found");

    std::vector<uint8_t> body = cached_firmware_blob("IVRS");
    if (body.empty()) {
        body.resize(table->size);
        if (!read_physical(table->phys_addr, body.data(), body.size()))
            return Result<DmarInfo>({}, "Failed to read IVRS table body");
    }
    auto info = parse_ivrs_table(body.data(), body.size());
    if (!info) return info;
    std::printf("[uefi] IVRS: %s\n", info->detail.c_str());
    return info;
}

// ═══════════════════════════════════════════════════════════════════════
// UEFI Memory Map
// ═══════════════════════════════════════════════════════════════════════

Result<std::vector<MemoryRegion>> get_memory_map(uint64_t /*rsdp_pa*/) {
    std::vector<MemoryRegion> regions;

#if LR_PLATFORM_LINUX
    // Binary EFI memmap may exist as directory of entries
    // /sys/firmware/efi/runtime-map/ or parse /proc/iomem as fallback
    FILE* map = std::fopen("/proc/iomem", "r");
    if (map) {
        char line[256];
        while (std::fgets(line, sizeof(line), map)) {
            unsigned long long start = 0, end = 0;
            char type_str[128] = {};
            if (std::sscanf(line, "%llx-%llx : %127[^\n]", &start, &end, type_str) >= 2) {
                // trim type_str
                MemoryRegion r;
                r.phys_start = start;
                r.phys_end = end;
                r.num_pages = (end >= start) ? ((end - start + 1) / 4096) : 0;
                r.is_available = (std::strstr(type_str, "System RAM") != nullptr);
                r.is_mmio = (std::strstr(type_str, "PCI") != nullptr ||
                             std::strstr(type_str, "MMIO") != nullptr);
                r.is_runtime = (std::strstr(type_str, "Reserved") != nullptr &&
                                std::strstr(type_str, "ACPI") != nullptr);
                if (std::strstr(type_str, "ACPI"))
                    r.type = static_cast<uint32_t>(EfiMemoryType::AcpiReclaimMemory);
                else if (r.is_available)
                    r.type = static_cast<uint32_t>(EfiMemoryType::ConventionalMemory);
                else if (r.is_mmio)
                    r.type = static_cast<uint32_t>(EfiMemoryType::MemoryMappedIO);
                else
                    r.type = static_cast<uint32_t>(EfiMemoryType::Reserved);
                regions.push_back(r);
            }
        }
        std::fclose(map);
    }
#elif LR_PLATFORM_WINDOWS
    // Usermode has no UEFI GetMemoryMap. Report honest error after optional
    // partial recovery is exhausted. Callers / tests use parse_efi_memory_map.
    (void)regions;
    return Result<std::vector<MemoryRegion>>({},
        "UEFI memory map not available from usermode on Windows "
        "(use parse_efi_memory_map on a captured descriptor buffer)");
#endif

    if (regions.empty()) {
        return Result<std::vector<MemoryRegion>>(
            {}, "UEFI memory map not available");
    }
    std::printf("[uefi] Memory map: %zu regions\n", regions.size());
    return Result<std::vector<MemoryRegion>>(std::move(regions));
}

} // namespace real::uefi

