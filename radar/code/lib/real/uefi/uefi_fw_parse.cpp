// uefi_fw_parse.cpp — Pure ACPI/EFI table parsers (no physical I/O).

#include "real/uefi/uefi_fw.hpp"
#include "real/uefi/uefi_phys.hpp"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <sstream>

namespace real::uefi {
bool AcpiRsdp::valid() const noexcept {
    if (std::memcmp(signature, "RSD PTR ", 8) != 0) return false;
    const auto* bytes = reinterpret_cast<const uint8_t*>(this);
    // ACPI 1.0: checksum first 20 bytes; ACPI 2.0+: also full length field
    if (!acpi_checksum_valid(bytes, 20)) return false;
    if (revision >= 2) {
        uint32_t len = length;
        if (len < 36 || len > sizeof(AcpiRsdp) + 8) {
            // Accept classic 36-byte RSDP even if length is weird when full struct sums
            len = 36;
        }
        if (len > sizeof(AcpiRsdp)) len = static_cast<uint32_t>(sizeof(AcpiRsdp));
        if (!acpi_checksum_valid(bytes, len)) return false;
    }
    return true;
}

Result<AcpiRsdp> parse_rsdp(const uint8_t* data, size_t size) {
    if (!data || size < 20)
        return Result<AcpiRsdp>({}, "RSDP buffer too small");
    if (std::memcmp(data, "RSD PTR ", 8) != 0)
        return Result<AcpiRsdp>({}, "RSDP signature mismatch");
    AcpiRsdp rsdp{};
    const size_t copy = std::min(size, sizeof(AcpiRsdp));
    std::memcpy(&rsdp, data, copy);
    if (!rsdp.valid())
        return Result<AcpiRsdp>({}, "RSDP checksum validation failed");
    return Result<AcpiRsdp>(rsdp);
}

Result<std::vector<uint64_t>> parse_sdt_entry_addresses(const uint8_t* sdt,
                                                        size_t sdt_len,
                                                        bool is_xsdt) {
    if (!sdt || sdt_len < sizeof(AcpiSdtHeader))
        return Result<std::vector<uint64_t>>({}, "SDT buffer too small");
    AcpiSdtHeader hdr{};
    std::memcpy(&hdr, sdt, sizeof(hdr));
    if (hdr.length < sizeof(AcpiSdtHeader) || hdr.length > sdt_len)
        return Result<std::vector<uint64_t>>({}, "SDT length invalid");
    if (!acpi_checksum_valid(sdt, hdr.length))
        return Result<std::vector<uint64_t>>({}, "SDT checksum invalid");

    const size_t entry_size = is_xsdt ? 8u : 4u;
    const size_t payload = hdr.length - sizeof(AcpiSdtHeader);
    if (payload % entry_size != 0)
        return Result<std::vector<uint64_t>>({}, "SDT entry array misaligned");

    std::vector<uint64_t> addrs;
    const size_t count = payload / entry_size;
    addrs.reserve(count);
    const uint8_t* p = sdt + sizeof(AcpiSdtHeader);
    for (size_t i = 0; i < count; ++i) {
        uint64_t pa = 0;
        if (is_xsdt) {
            std::memcpy(&pa, p + i * 8, 8);
        } else {
            uint32_t a32 = 0;
            std::memcpy(&a32, p + i * 4, 4);
            pa = a32;
        }
        if (pa) addrs.push_back(pa);
    }
    return Result<std::vector<uint64_t>>(std::move(addrs));
}

Result<DmarInfo> parse_dmar_table(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(AcpiDmarHeader))
        return Result<DmarInfo>({}, "DMAR buffer too small");
    if (std::memcmp(data, "DMAR", 4) != 0)
        return Result<DmarInfo>({}, "DMAR signature mismatch");

    AcpiDmarHeader dmar{};
    std::memcpy(&dmar, data, sizeof(dmar));
    if (dmar.header.length < sizeof(AcpiDmarHeader) || dmar.header.length > size)
        return Result<DmarInfo>({}, "DMAR length invalid");
    if (!acpi_checksum_valid(data, dmar.header.length))
        return Result<DmarInfo>({}, "DMAR checksum invalid");

    DmarInfo info;
    info.host_address_width = dmar.host_address_width;
    // Flags bit0: INTR_REMAP; DMA remapping is present when DRHD structures exist.
    // Platforms may leave remapping disabled in hardware — we report table presence
    // and DRHD coverage; dma_remap_active true when at least one DRHD is present.
    info.dma_remap_active = false;

    size_t pos = sizeof(AcpiDmarHeader);
    const size_t end = dmar.header.length;
    while (pos + 4 <= end) {
        uint16_t type = 0, length = 0;
        std::memcpy(&type, data + pos, 2);
        std::memcpy(&length, data + pos + 2, 2);
        if (length < 4 || pos + length > end) {
            return Result<DmarInfo>({}, "DMAR structure length overrun");
        }
        if (type == kDmarTypeDrhd) {
            if (length < 16)
                return Result<DmarInfo>({}, "DRHD structure too short");
            AcpiDmarDrhd drhd{};
            std::memcpy(&drhd, data + pos, std::min(sizeof(drhd), static_cast<size_t>(length)));
            info.drhd_count++;
            info.dma_remap_active = true;
            if (drhd.flags & 0x1) info.include_all_pci = true;
            if (info.drhd_count == 1) info.base_address = drhd.base_address;
        } else if (type == kDmarTypeRmrr) {
            info.rmrr_count++;
        }
        pos += length;
    }

    std::ostringstream oss;
    oss << "DMAR width=" << static_cast<int>(info.host_address_width)
        << " DRHD=" << info.drhd_count
        << " RMRR=" << info.rmrr_count
        << " INCLUDE_ALL=" << (info.include_all_pci ? "yes" : "no")
        << " base=0x" << std::hex << info.base_address;
    info.detail = oss.str();
    return Result<DmarInfo>(std::move(info));
}

Result<DmarInfo> parse_ivrs_table(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(AcpiIvrsHeader))
        return Result<DmarInfo>({}, "IVRS buffer too small");
    if (std::memcmp(data, "IVRS", 4) != 0)
        return Result<DmarInfo>({}, "IVRS signature mismatch");

    AcpiIvrsHeader ivrs{};
    std::memcpy(&ivrs, data, sizeof(ivrs));
    if (ivrs.header.length < sizeof(AcpiIvrsHeader) || ivrs.header.length > size)
        return Result<DmarInfo>({}, "IVRS length invalid");
    if (!acpi_checksum_valid(data, ivrs.header.length))
        return Result<DmarInfo>({}, "IVRS checksum invalid");

    DmarInfo info;
    // AMD IVRS IV_Info layout varies by revision; only accept a plausible PA width
    // (32..64). Otherwise leave 0 and keep raw IV_Info in detail.
    {
        const uint8_t pasize = static_cast<uint8_t>((ivrs.iv_info >> 8) & 0x7F);
        if (pasize >= 32 && pasize <= 64)
            info.host_address_width = pasize;
    }
    info.dma_remap_active = false;

    size_t pos = sizeof(AcpiIvrsHeader);
    const size_t end = ivrs.header.length;
    while (pos + 4 <= end) {
        const uint8_t type = data[pos];
        uint16_t length = 0;
        // IVHD type 10h: length at offset 2 (uint16); IVMD similar
        if (pos + 4 > end) break;
        std::memcpy(&length, data + pos + 2, 2);
        if (length < 4 || pos + length > end)
            return Result<DmarInfo>({}, "IVRS IVxx structure length overrun");

        if (type == kIvhdType10 || type == kIvhdType11) {
            info.drhd_count++; // reuse field as IVHD count
            info.dma_remap_active = true;
            // Base address at offset 8 for type 10h (uint64)
            if (length >= 16 && info.drhd_count == 1) {
                std::memcpy(&info.base_address, data + pos + 8, 8);
            }
        } else if (type == kIvmdType20 || type == kIvmdType21 || type == kIvmdType22) {
            info.rmrr_count++;
        }
        pos += length;
    }

    std::ostringstream oss;
    oss << "IVRS IV_Info=0x" << std::hex << ivrs.iv_info
        << std::dec << " IVHD=" << info.drhd_count
        << " IVMD=" << info.rmrr_count
        << " base=0x" << std::hex << info.base_address;
    info.detail = oss.str();
    return Result<DmarInfo>(std::move(info));
}

Result<EfiSystemTable> parse_efi_system_table(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(EfiTableHeader) + 8)
        return Result<EfiSystemTable>({}, "EFI system table buffer too small");
    EfiSystemTable st{};
    const size_t copy = std::min(size, sizeof(EfiSystemTable));
    std::memcpy(&st, data, copy);
    if (st.header.signature != kEfiSystemTableSignature)
        return Result<EfiSystemTable>({}, "EFI system table signature mismatch");
    if (st.header.header_size < sizeof(EfiTableHeader))
        return Result<EfiSystemTable>({}, "EFI system table header_size invalid");
    return Result<EfiSystemTable>(st);
}

Result<std::vector<ConfigTableEntry>> parse_config_table_array(
    const uint8_t* data, size_t size, size_t entry_count) {
    if (!data)
        return Result<std::vector<ConfigTableEntry>>({}, "null config table buffer");
    const size_t need = entry_count * sizeof(EfiConfigurationTable);
    if (size < need)
        return Result<std::vector<ConfigTableEntry>>({},
            "config table buffer smaller than entry_count");
    std::vector<ConfigTableEntry> result;
    result.reserve(entry_count);
    for (size_t i = 0; i < entry_count; ++i) {
        EfiConfigurationTable ct{};
        std::memcpy(&ct, data + i * sizeof(EfiConfigurationTable), sizeof(ct));
        result.push_back({ct.vendor_guid_hi, ct.vendor_guid_lo, ct.vendor_table});
    }
    return Result<std::vector<ConfigTableEntry>>(std::move(result));
}

Result<std::vector<MemoryRegion>> parse_efi_memory_map(const uint8_t* data,
                                                       size_t size,
                                                       size_t descriptor_size) {
    if (!data || size == 0)
        return Result<std::vector<MemoryRegion>>({}, "empty memory map buffer");
    if (descriptor_size < 40 || descriptor_size > 64)
        return Result<std::vector<MemoryRegion>>({}, "invalid EFI descriptor size");
    if (size % descriptor_size != 0)
        return Result<std::vector<MemoryRegion>>({}, "memory map size not multiple of descriptor");

    std::vector<MemoryRegion> regions;
    const size_t count = size / descriptor_size;
    regions.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const uint8_t* p = data + i * descriptor_size;
        EfiMemoryDescriptor d{};
        std::memcpy(&d, p, std::min(sizeof(d), descriptor_size));
        MemoryRegion r;
        r.type = d.type;
        r.phys_start = d.physical_start;
        r.num_pages = d.number_of_pages;
        r.phys_end = d.physical_start + d.number_of_pages * 4096ull;
        if (r.phys_end) r.phys_end -= 1;
        r.attributes = d.attribute;
        r.is_available = (d.type == static_cast<uint32_t>(EfiMemoryType::ConventionalMemory));
        r.is_mmio = (d.type == static_cast<uint32_t>(EfiMemoryType::MemoryMappedIO) ||
                     d.type == static_cast<uint32_t>(EfiMemoryType::MemoryMappedIOPortSpace));
        r.is_runtime = (d.attribute & 0x8000000000000000ull) != 0; // EFI_MEMORY_RUNTIME
        regions.push_back(r);
    }
    return Result<std::vector<MemoryRegion>>(std::move(regions));
}

} // namespace real::uefi

