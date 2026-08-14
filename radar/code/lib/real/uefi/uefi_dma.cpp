// uefi_dma.cpp — UEFI DMA remapping detection and IOMMU status.

#include "real/uefi/uefi_dma.hpp"
#include "real/uefi/uefi_fw.hpp"
#include "real/uefi/uefi_phys.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <unistd.h>
#  include <dirent.h>
#elif LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#  include <setupapi.h>
#  include <devguid.h>
#  pragma comment(lib, "setupapi.lib")
#endif

namespace real::uefi {
namespace {

Result<McfgInfo> parse_mcfg_bytes(const uint8_t* data, size_t size) {
    // MCFG: AcpiSdtHeader (36) + reserved 8 + allocations (16 each)
    if (!data || size < 44)
        return Result<McfgInfo>({}, "MCFG buffer too small");
    if (std::memcmp(data, "MCFG", 4) != 0)
        return Result<McfgInfo>({}, "MCFG signature mismatch");
    uint32_t length = 0;
    std::memcpy(&length, data + 4, 4);
    if (length < 44 || length > size)
        return Result<McfgInfo>({}, "MCFG length invalid");
    if (!acpi_checksum_valid(data, length))
        return Result<McfgInfo>({}, "MCFG checksum invalid");
    if (length < 44 + sizeof(AcpiMcfgAllocation))
        return Result<McfgInfo>({}, "MCFG has no allocation entries");

    AcpiMcfgAllocation alloc{};
    std::memcpy(&alloc, data + 44, sizeof(alloc));
    McfgInfo info;
    info.base_address = alloc.base_address;
    info.segment_group = alloc.segment_group;
    info.start_bus = alloc.start_bus;
    info.end_bus = alloc.end_bus;
    if (!info.base_address)
        return Result<McfgInfo>({}, "MCFG base_address is zero");
    return Result<McfgInfo>(info);
}

#if LR_PLATFORM_WINDOWS
bool pci_vid_present(uint16_t want_vid, uint16_t want_did, bool match_did) {
    HDEVINFO devs = SetupDiGetClassDevsA(nullptr, "PCI", nullptr,
                                         DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (devs == INVALID_HANDLE_VALUE) return false;
    SP_DEVINFO_DATA info{};
    info.cbSize = sizeof(info);
    bool found = false;
    for (DWORD i = 0; SetupDiEnumDeviceInfo(devs, i, &info); ++i) {
        char id[256]{};
        if (!SetupDiGetDeviceInstanceIdA(devs, &info, id, sizeof(id), nullptr))
            continue;
        // PCI\VEN_10EE&DEV_9034&...
        unsigned vid = 0, did = 0;
        if (std::sscanf(id, "PCI\\VEN_%x&DEV_%x", &vid, &did) >= 1) {
            if (static_cast<uint16_t>(vid) == want_vid) {
                if (!match_did || static_cast<uint16_t>(did) == want_did) {
                    found = true;
                    break;
                }
            }
        }
    }
    SetupDiDestroyDeviceInfoList(devs);
    return found;
}
#endif

} // namespace

Result<bool> dma_remapping_active() {
    auto dmar = read_dmar_table();
    if (dmar) return Result<bool>(dmar->dma_remap_active);
    auto ivrs = read_ivrs_table();
    if (ivrs) return Result<bool>(ivrs->dma_remap_active);
    return Result<bool>(false,
        std::string("No IOMMU tables found (DMAR: ") + dmar.error_msg.c_str() + ")");
}

Result<DmaProtectionInfo> get_dma_protection_info() {
    DmaProtectionInfo info;
    std::string errors;

    auto dmar = read_dmar_table();
    if (dmar) {
        info.vt_d_active = dmar->dma_remap_active;
        info.drhd_count = dmar->drhd_count;
        info.address_width = dmar->host_address_width;
        info.drhd_all_devices = dmar->include_all_pci;
    } else {
        errors += std::string("DMAR=") + dmar.error_msg.c_str() + "; ";
    }

    auto ivrs = read_ivrs_table();
    if (ivrs) {
        info.amd_vi_active = ivrs->dma_remap_active;
        if (!info.drhd_count) info.drhd_count = ivrs->drhd_count;
        if (!info.address_width) info.address_width = ivrs->host_address_width;
    } else {
        errors += std::string("IVRS=") + ivrs.error_msg.c_str() + "; ";
    }

    info.iommu_bypass_available = !info.vt_d_active && !info.amd_vi_active;

    auto tb = thunderbolt_dma_available();
    if (tb) {
        info.thunderbolt_active = *tb;
    } else {
        info.thunderbolt_active = false;
        errors += std::string("TB=") + tb.error_msg.c_str() + "; ";
    }

    info.detail = std::string("VT-d=") + (info.vt_d_active ? "ON" : "OFF") +
                  " AMD-Vi=" + (info.amd_vi_active ? "ON" : "OFF") +
                  " DRHD=" + std::to_string(info.drhd_count) +
                  " ALL_PCI=" + (info.drhd_all_devices ? "yes" : "no") +
                  " ThunderboltDMA=" + (info.thunderbolt_active ? "OPEN" : "no") +
                  " bypass=" + (info.iommu_bypass_available ? "possible" : "blocked");
    if (!errors.empty())
        info.detail += " [" + errors + "]";

    std::printf("[uefi:dma] %s\n", info.detail.c_str());
    return Result<DmaProtectionInfo>(std::move(info));
}

Result<bool> thunderbolt_dma_available() {
#if LR_PLATFORM_LINUX
    // Domain security level: none|user|secure|dponly
    FILE* sl = std::fopen("/sys/bus/thunderbolt/devices/domain0/security", "r");
    if (!sl) {
        // Older path
        sl = std::fopen("/sys/bus/thunderbolt/security", "r");
    }
    if (sl) {
        char buf[64] = {};
        if (std::fgets(buf, sizeof(buf), sl)) {
            std::fclose(sl);
            // Strip newline
            for (char* p = buf; *p; ++p) if (*p == '\n' || *p == '\r') *p = 0;
            std::printf("[uefi:dma] Thunderbolt security: %s\n", buf);
            // "none" / level 0 => DMA always available
            if (std::strcmp(buf, "none") == 0 || std::strcmp(buf, "0") == 0)
                return Result<bool>(true);
            if (std::strcmp(buf, "user") == 0 || std::strcmp(buf, "1") == 0)
                return Result<bool>(true); // user-approvable tunnels
            // secure / dponly => not freely available
            return Result<bool>(false);
        }
        std::fclose(sl);
    }
    if (access("/sys/bus/thunderbolt/devices/", F_OK) == 0) {
        std::printf("[uefi:dma] Thunderbolt bus present, security node missing\n");
        return Result<bool>(true);
    }
    return Result<bool>(false, "Thunderbolt not detected");
#elif LR_PLATFORM_WINDOWS
    // Probe SetupAPI for Thunderbolt controllers (Intel class / common VEN)
    HDEVINFO devs = SetupDiGetClassDevsA(nullptr, "PCI", nullptr,
                                         DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (devs == INVALID_HANDLE_VALUE)
        return Result<bool>(false, "SetupDiGetClassDevs failed for Thunderbolt probe");

    SP_DEVINFO_DATA info{};
    info.cbSize = sizeof(info);
    bool tb_present = false;
    for (DWORD i = 0; SetupDiEnumDeviceInfo(devs, i, &info); ++i) {
        char id[256]{};
        char desc[256]{};
        if (!SetupDiGetDeviceInstanceIdA(devs, &info, id, sizeof(id), nullptr))
            continue;
        SetupDiGetDeviceRegistryPropertyA(devs, &info, SPDRP_DEVICEDESC,
                                          nullptr, reinterpret_cast<PBYTE>(desc),
                                          sizeof(desc), nullptr);
        // Thunderbolt / USB4 controllers
        if (std::strstr(desc, "Thunderbolt") || std::strstr(desc, "USB4") ||
            std::strstr(id, "VEN_8086&DEV_15") || // common Alpine Ridge etc.
            std::strstr(id, "VEN_8086&DEV_113")) {
            tb_present = true;
            break;
        }
    }
    SetupDiDestroyDeviceInfoList(devs);

    if (!tb_present)
        return Result<bool>(false, "Thunderbolt/USB4 controller not detected");

    // Without kernel DMA protection query, presence of TB ports implies
    // potential DMA if Kernel DMA Protection is off. Check registry/policy.
    // HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\DmaSecurity
    HKEY key = nullptr;
    DWORD dma_prot = 0;
    DWORD sz = sizeof(dma_prot);
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "SYSTEM\\CurrentControlSet\\Control\\DmaSecurity\\AllowedBuses",
            0, KEY_READ, &key) == ERROR_SUCCESS) {
        RegCloseKey(key);
        // Key exists — policy present; treat as protected unless empty probe fails
        std::printf("[uefi:dma] Thunderbolt present; DmaSecurity policy key exists\n");
        return Result<bool>(false); // Kernel DMA protection policy configured
    }
    std::printf("[uefi:dma] Thunderbolt present; no DmaSecurity AllowedBuses key\n");
    return Result<bool>(true);
#else
    return Result<bool>(false, "Thunderbolt probe unsupported");
#endif
}

Result<McfgInfo> read_mcfg_table() {
    auto table = find_acpi_table("MCFG");
    if (!table) return Result<McfgInfo>({}, "MCFG table not found");

    std::vector<uint8_t> body = cached_firmware_blob("MCFG");
    if (body.empty()) {
        body.resize(table->size);
        if (!read_physical(table->phys_addr, body.data(), body.size()))
            return Result<McfgInfo>({}, "Cannot read MCFG table body");
    }
    auto info = parse_mcfg_bytes(body.data(), body.size());
    if (!info) return info;
    std::printf("[uefi:dma] MCFG: base=0x%llx buses=%u-%u seg=%u\n",
                static_cast<unsigned long long>(info->base_address),
                info->start_bus, info->end_bus, info->segment_group);
    return info;
}

// Exported pure parse for tests / callers that already hold bytes
Result<McfgInfo> parse_mcfg_table(const uint8_t* data, size_t size) {
    return parse_mcfg_bytes(data, size);
}

Result<bool> fpga_dma_device_present() {
    constexpr uint16_t kVendorXilinx = 0x10EE;
    constexpr uint16_t kDevicePcileech = 0x9034;

#if LR_PLATFORM_WINDOWS
    if (pci_vid_present(kVendorXilinx, kDevicePcileech, true)) {
        std::printf("[uefi:dma] FPGA DMA device (Xilinx 10EE:9034) present via SetupAPI\n");
        return Result<bool>(true);
    }
    if (pci_vid_present(kVendorXilinx, 0, false)) {
        std::printf("[uefi:dma] Xilinx FPGA present (non-9034 DID) via SetupAPI\n");
        return Result<bool>(true);
    }
#endif

#if LR_PLATFORM_LINUX
    // sysfs PCI devices
    DIR* dir = opendir("/sys/bus/pci/devices");
    if (dir) {
        while (dirent* ent = readdir(dir)) {
            if (ent->d_name[0] == '.') continue;
            char path[512];
            std::snprintf(path, sizeof(path),
                          "/sys/bus/pci/devices/%s/vendor", ent->d_name);
            FILE* vf = std::fopen(path, "r");
            if (!vf) continue;
            unsigned vendor = 0;
            if (std::fscanf(vf, "%x", &vendor) != 1) { std::fclose(vf); continue; }
            std::fclose(vf);
            if (vendor != kVendorXilinx) continue;
            std::snprintf(path, sizeof(path),
                          "/sys/bus/pci/devices/%s/device", ent->d_name);
            FILE* df = std::fopen(path, "r");
            unsigned device = 0;
            if (df) {
                std::fscanf(df, "%x", &device);
                std::fclose(df);
            }
            std::printf("[uefi:dma] Xilinx device at %s id=%04x\n", ent->d_name, device);
            closedir(dir);
            return Result<bool>(true);
        }
        closedir(dir);
    }
#endif

    // MCFG ECAM probe (requires physical read of config space)
    auto mcfg = read_mcfg_table();
    if (!mcfg) {
        return Result<bool>(false,
            std::string("No FPGA DMA device (MCFG unavailable: ") +
            mcfg.error_msg.c_str() + ")");
    }

    for (unsigned bus = mcfg->start_bus; bus <= mcfg->end_bus; ++bus) {
        for (unsigned dev = 0; dev < 32; ++dev) {
            for (unsigned fn = 0; fn < 8; ++fn) {
                const uint64_t config_addr = mcfg->base_address +
                    (static_cast<uint64_t>(bus) << 20) |
                    (static_cast<uint64_t>(dev) << 15) |
                    (static_cast<uint64_t>(fn) << 12);
                uint16_t vendor = 0;
                if (!read_physical(config_addr, &vendor, sizeof(vendor))) {
                    // If we cannot read any config, stop early on first bus
                    if (bus == mcfg->start_bus && dev == 0 && fn == 0) {
                        return Result<bool>(false,
                            "PCIe ECAM physical read denied; cannot scan for FPGA DMA");
                    }
                    continue;
                }
                if (vendor == 0xFFFF || vendor == 0) continue;
                if (vendor == kVendorXilinx) {
                    uint16_t device = 0;
                    read_physical(config_addr + 2, &device, sizeof(device));
                    std::printf("[uefi:dma] FPGA DMA candidate at %02x:%02x.%u did=%04x\n",
                                bus, dev, fn, device);
                    return Result<bool>(true);
                }
            }
        }
    }
    return Result<bool>(false, "No FPGA DMA device on PCIe bus");
}

} // namespace real::uefi
