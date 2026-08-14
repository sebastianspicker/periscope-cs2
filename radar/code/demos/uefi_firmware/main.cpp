// main.cpp — UEFI firmware analysis demo.
//
// Runs the full UEFI firmware analysis pipeline:
//   --scan       Full firmware analysis
//   --secureboot Secure Boot state
//   --dma        DMA remapping / IOMMU state
//   --txt        Intel TXT / TPM event log
//   --acpi       List all ACPI tables
//   --mcfg       PCIe MCFG config space info
//   --dmar       DMAR table details
//   --pcie-dev   Scan for FPGA DMA device
//   --tpm        TPM event log
//
// BUILD: enable -DLR_ENABLE_REAL_UEFI=ON and build target uefi_fw
// RUN:   uefi_fw --scan

#include "real/uefi/uefi_lab.hpp"
#include <cstdio>
#include <cstring>

static int usage(const char* argv0) {
    std::printf("UEFI Firmware Analysis Tool\n");
    std::printf("Usage: %s <command>\n", argv0);
    std::printf("  --scan       Full firmware analysis\n");
    std::printf("  --dma        DMA remapping / IOMMU state\n");
    std::printf("  --secureboot Secure Boot variables\n");
    std::printf("  --txt        Intel TXT / measured launch\n");
    std::printf("  --tpm        TPM event log\n");
    std::printf("  --acpi       List ACPI tables\n");
    std::printf("  --dmar       DMAR table details\n");
    std::printf("  --mcfg       MCFG / PCIe ECAM base\n");
    std::printf("  --pcie-dev   Scan for FPGA DMA device\n");
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) return usage(argv[0]);

    if (std::strcmp(argv[1], "--scan") == 0) {
        auto report = real::uefi::lab::analyze_firmware();
        real::uefi::lab::print_report(report);
        return 0;
    }
    if (std::strcmp(argv[1], "--dma") == 0) {
        auto info = real::uefi::get_dma_protection_info();
        if (info) {
            std::printf("DMA Protection: %s\n", info->detail.c_str());
            std::printf("  VT-d active:     %s\n", info->vt_d_active ? "yes" : "no");
            std::printf("  AMD-Vi active:   %s\n", info->amd_vi_active ? "yes" : "no");
            std::printf("  DRHD/IVHD units: %d\n", info->drhd_count);
            std::printf("  INCLUDE_PCI_ALL: %s\n", info->drhd_all_devices ? "yes" : "no");
            std::printf("  Thunderbolt DMA: %s\n", info->thunderbolt_active ? "OPEN" : "no");
            std::printf("  IOMMU bypass:    %s\n",
                        info->iommu_bypass_available ? "possible" : "blocked");
        } else {
            std::printf("DMA protection query failed: %s\n", info.error_msg.c_str());
        }
        return 0;
    }
    if (std::strcmp(argv[1], "--secureboot") == 0) {
        auto sb = real::uefi::get_secure_boot_state();
        if (sb) {
            std::printf("Secure Boot: %s\n", sb->secure_boot_enabled ? "ON" : "OFF");
            std::printf("Setup Mode:  %s\n", sb->setup_mode ? "ON" : "OFF");
            std::printf("Audit Mode:  %s\n", sb->audit_mode ? "ON" : "OFF");
            std::printf("Deployed:    %s\n", sb->deployed_mode ? "ON" : "OFF");
            std::printf("PK=%zu KEK=%zu db=%zu dbx=%zu bytes\n",
                        sb->pk.size(), sb->kek.size(), sb->db.size(), sb->dbx.size());
        } else {
            std::printf("Secure Boot state: %s\n", sb.error_msg.c_str());
            std::printf("  (partial) SecureBoot=%d SetupMode=%d\n",
                        sb.value.secure_boot_enabled ? 1 : 0,
                        sb.value.setup_mode ? 1 : 0);
        }
        return 0;
    }
    if (std::strcmp(argv[1], "--txt") == 0) {
        auto txt = real::uefi::read_intel_txt_data();
        if (txt) {
            std::printf("TXT Capable: %d\n", txt->txt_capable ? 1 : 0);
            std::printf("TXT Enabled: %d\n", txt->txt_enabled ? 1 : 0);
            std::printf("SMX:         %d\n", txt->smx_enabled ? 1 : 0);
            std::printf("Launch:      %d\n", txt->measured_launch_occurred ? 1 : 0);
            std::printf("Heap base:   0x%llx\n",
                        static_cast<unsigned long long>(txt->heap_base));
            if (!txt->detail.empty())
                std::printf("Detail:      %s\n", txt->detail.c_str());
        } else {
            std::printf("TXT query failed: %s\n", txt.error_msg.c_str());
        }
        return 0;
    }
    if (std::strcmp(argv[1], "--tpm") == 0) {
        auto events = real::uefi::read_tpm_event_log();
        if (events) {
            std::printf("TPM Event Log: %zu entries\n", events->size());
            for (auto& e : *events) {
                std::printf("  PCR[%u] type=0x%x digests=%u %s\n",
                            e.pcr_index, e.event_type, e.digest_count,
                            e.description.c_str());
            }
        } else {
            std::printf("TPM event log not available: %s\n", events.error_msg.c_str());
        }
        return 0;
    }
    if (std::strcmp(argv[1], "--acpi") == 0) {
        auto rsdp = real::uefi::find_rsdp_pa();
        if (!rsdp) {
            std::printf("RSDP not found: %s\n", rsdp.error_msg.c_str());
            return 1;
        }
        std::printf("RSDP PA: 0x%llx\n", static_cast<unsigned long long>(*rsdp));
        auto tables = real::uefi::enumerate_acpi_tables(*rsdp);
        if (!tables) {
            std::printf("ACPI enumeration failed: %s\n", tables.error_msg.c_str());
            return 1;
        }
        for (auto& t : *tables) {
            std::printf("  %-4s at 0x%llx (%u bytes)\n",
                        t.signature.c_str(),
                        static_cast<unsigned long long>(t.phys_addr), t.size);
        }
        return 0;
    }
    if (std::strcmp(argv[1], "--dmar") == 0) {
        auto dmar = real::uefi::read_dmar_table();
        if (dmar) {
            std::printf("DMAR: %s\n", dmar->detail.c_str());
            std::printf("  Width: %d, DRHD: %d, RMRR: %d, Active: %d\n",
                        dmar->host_address_width, dmar->drhd_count,
                        dmar->rmrr_count, dmar->dma_remap_active ? 1 : 0);
        } else {
            std::printf("DMAR not available: %s\n", dmar.error_msg.c_str());
        }
        return 0;
    }
    if (std::strcmp(argv[1], "--mcfg") == 0) {
        auto mcfg = real::uefi::read_mcfg_table();
        if (mcfg) {
            std::printf("MCFG base=0x%llx seg=%u buses=%u-%u\n",
                        static_cast<unsigned long long>(mcfg->base_address),
                        mcfg->segment_group, mcfg->start_bus, mcfg->end_bus);
        } else {
            std::printf("MCFG not available: %s\n", mcfg.error_msg.c_str());
        }
        return 0;
    }
    if (std::strcmp(argv[1], "--pcie-dev") == 0) {
        auto fpga = real::uefi::fpga_dma_device_present();
        if (fpga) {
            std::printf("FPGA DMA device: %s\n", *fpga ? "PRESENT" : "Not found");
        } else {
            std::printf("FPGA DMA scan: %s\n", fpga.error_msg.c_str());
        }
        return 0;
    }

    std::printf("Unknown command: %s\n", argv[1]);
    usage(argv[0]);
    return 1;
}
