// uefi_lab.cpp — UEFI laboratory: comprehensive firmware analysis.

#include "real/uefi/uefi_lab.hpp"
#include <cstdio>

namespace real::uefi::lab {

FirmwareReport analyze_firmware() {
    FirmwareReport report;

    // ─── Phase 1: RSDP ────────────────────────────────────────────
    auto rsdp_pa = find_rsdp_pa();
    if (rsdp_pa) {
        report.rsdp.found = true;
        report.rsdp.pa = *rsdp_pa;
        auto rsdp = read_rsdp(*rsdp_pa);
        if (rsdp) {
            report.rsdp.revision = rsdp->revision;
        } else {
            report.error += std::string("RSDP read: ") + rsdp.error_msg.c_str() + "; ";
        }
    } else {
        report.error += std::string("RSDP not found: ") + rsdp_pa.error_msg.c_str() + "; ";
    }

    // ─── Phase 2: ACPI tables ──────────────────────────────────────
    if (rsdp_pa) {
        auto tables = enumerate_acpi_tables(*rsdp_pa);
        if (tables) {
            report.acpi.sdt_count = static_cast<int>(tables->size());
            for (auto& t : *tables) {
                if (t.signature == "DMAR") report.acpi.has_dmar = true;
                if (t.signature == "IVRS") report.acpi.has_ivrs = true;
                if (t.signature == "MCFG") report.acpi.has_mcfg = true;
                if (t.signature == "SSDT") report.acpi.has_ssdt = true;
                if (t.signature == "TPM2" || t.signature == "TCPA")
                    report.acpi.has_tpm2 = true;
            }
            report.acpi.detail = std::to_string(tables->size()) + " ACPI tables";
        } else {
            report.error += std::string("ACPI enum: ") + tables.error_msg.c_str() + "; ";
            report.acpi.detail = tables.error_msg.c_str();
        }
    }

    // ─── Phase 3: UEFI System Table ────────────────────────────────
    if (rsdp_pa) {
        auto st = read_efi_system_table(*rsdp_pa);
        if (st) {
            report.uefi.system_table_found = true;
            report.uefi.runtime_services_pa = st->runtime_services;
            auto ct = enumerate_config_table(*st);
            if (ct) {
                report.uefi.config_table_entries = static_cast<int>(ct->size());
            } else {
                report.error += std::string("config table: ") + ct.error_msg.c_str() + "; ";
            }
        } else {
            report.error += std::string("EFI ST: ") + st.error_msg.c_str() + "; ";
        }
    }

    // ─── Phase 4: DMA / IOMMU ─────────────────────────────────────
    auto dma = get_dma_protection_info();
    if (dma) {
        report.dma.active = dma->vt_d_active || dma->amd_vi_active;
        report.dma.drhd_count = dma->drhd_count;
        report.dma.address_width = dma->address_width;
        report.dma.detail = dma->detail;

        auto tb = thunderbolt_dma_available();
        if (tb) {
            report.dma.thunderbolt_dma = *tb;
        } else {
            report.dma.thunderbolt_dma = false;
            report.error += std::string("Thunderbolt: ") + tb.error_msg.c_str() + "; ";
        }

        auto fpga = fpga_dma_device_present();
        if (fpga) {
            report.dma.fpga_dma_present = *fpga;
        } else {
            report.dma.fpga_dma_present = false;
            // absence is normal — keep message in detail only
            report.dma.detail += std::string(" | FPGA: ") + fpga.error_msg.c_str();
        }
    } else {
        report.error += std::string("DMA info: ") + dma.error_msg.c_str() + "; ";
    }

    // ─── Phase 5: Secure Boot ─────────────────────────────────────
    auto sb = get_secure_boot_state();
    if (sb) {
        report.secure_boot.secure_boot_on = sb->secure_boot_enabled;
        report.secure_boot.setup_mode = sb->setup_mode;
        report.secure_boot.audit_mode = sb->audit_mode;
        report.secure_boot.deployed_mode = sb->deployed_mode;
        report.secure_boot.pk_bytes = static_cast<int>(sb->pk.size());
        report.secure_boot.kek_bytes = static_cast<int>(sb->kek.size());
        report.secure_boot.db_bytes = static_cast<int>(sb->db.size());
        report.secure_boot.dbx_bytes = static_cast<int>(sb->dbx.size());
    } else {
        // Partial fields may still be in value when ok=false
        report.secure_boot.secure_boot_on = sb.value.secure_boot_enabled;
        report.secure_boot.setup_mode = sb.value.setup_mode;
        report.error += std::string("Secure Boot: ") + sb.error_msg.c_str() + "; ";
    }

    // ─── Phase 6: TXT / TPM ──────────────────────────────────────
    auto txt = read_intel_txt_data();
    if (txt) {
        report.txt.txt_capable = txt->txt_capable;
        report.txt.txt_enabled = txt->txt_enabled;
        report.txt.measured_launch = txt->measured_launch_occurred;
        report.txt.detail = txt->detail;
    } else {
        report.error += std::string("TXT: ") + txt.error_msg.c_str() + "; ";
    }

    auto events = read_tpm_event_log();
    if (events) {
        report.txt.tpm_events = static_cast<int>(events->size());
    } else {
        report.error += std::string("TPM log: ") + events.error_msg.c_str() + "; ";
    }

    // Memory map (best-effort)
    if (rsdp_pa) {
        auto mm = get_memory_map(*rsdp_pa);
        if (mm) {
            report.memory.region_count = static_cast<int>(mm->size());
        } else {
            report.memory.detail = mm.error_msg.c_str();
        }
    }

    return report;
}

void print_report(const FirmwareReport& r) {
    std::printf("\n═══════════════════════════════════════════════\n");
    std::printf("  UEFI Firmware Analysis Report\n");
    std::printf("═══════════════════════════════════════════════\n\n");

    std::printf("  RSDP:          %s (PA=0x%llx rev=%d)\n",
                r.rsdp.found ? "FOUND" : "NOT FOUND",
                static_cast<unsigned long long>(r.rsdp.pa), r.rsdp.revision);
    std::printf("  ACPI tables:   %d (%s)\n", r.acpi.sdt_count, r.acpi.detail.c_str());
    if (r.acpi.has_dmar) std::printf("    - DMAR (VT-d IOMMU table)\n");
    if (r.acpi.has_ivrs) std::printf("    - IVRS (AMD-Vi IOMMU table)\n");
    if (r.acpi.has_mcfg) std::printf("    - MCFG (PCIe config space)\n");
    if (r.acpi.has_tpm2) std::printf("    - TPM2/TCPA (TPM configuration)\n");
    if (r.acpi.has_ssdt) std::printf("    - SSDT present\n");

    std::printf("  UEFI System:   %s\n",
                r.uefi.system_table_found ? "FOUND" : "NOT FOUND");
    if (r.uefi.runtime_services_pa)
        std::printf("    RT services: 0x%llx\n",
                    static_cast<unsigned long long>(r.uefi.runtime_services_pa));
    std::printf("    Config table: %d entries\n", r.uefi.config_table_entries);

    std::printf("  DMA Protection: %s\n", r.dma.active ? "ACTIVE" : "OFF/UNKNOWN");
    if (!r.dma.detail.empty())
        std::printf("    Detail:      %s\n", r.dma.detail.c_str());
    if (r.dma.drhd_count) std::printf("    DRHD/IVHD:   %d\n", r.dma.drhd_count);
    if (r.dma.address_width) std::printf("    Addr width:  %d bits\n", r.dma.address_width);
    std::printf("    Thunderbolt:  %s\n", r.dma.thunderbolt_dma ? "DMA CAPABLE" : "Protected/Absent");
    std::printf("    FPGA device:  %s\n", r.dma.fpga_dma_present ? "PRESENT" : "Not found");

    std::printf("  Secure Boot:    %s\n", r.secure_boot.secure_boot_on ? "ON" : "OFF");
    if (r.secure_boot.setup_mode)
        std::printf("    Setup Mode (PK not enrolled)\n");
    if (r.secure_boot.audit_mode)
        std::printf("    Audit Mode\n");
    if (r.secure_boot.deployed_mode)
        std::printf("    Deployed Mode\n");
    std::printf("    PK=%d KEK=%d db=%d dbx=%d bytes\n",
                r.secure_boot.pk_bytes, r.secure_boot.kek_bytes,
                r.secure_boot.db_bytes, r.secure_boot.dbx_bytes);

    std::printf("  Intel TXT:      capable=%d enabled=%d launch=%d\n",
                r.txt.txt_capable, r.txt.txt_enabled, r.txt.measured_launch);
    if (!r.txt.detail.empty())
        std::printf("    Detail:      %s\n", r.txt.detail.c_str());
    std::printf("  TPM events:     %d\n", r.txt.tpm_events);

    std::printf("  Memory map:     %d regions", r.memory.region_count);
    if (!r.memory.detail.empty())
        std::printf(" (%s)", r.memory.detail.c_str());
    std::printf("\n");

    if (!r.error.empty())
        std::printf("\n  NOTES/ERRORS: %s\n", r.error.c_str());

    std::printf("\n═══════════════════════════════════════════════\n");
}

} // namespace real::uefi::lab
