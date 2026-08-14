// evasion_t4_advanced.cpp — multi-step T4 off-box stealth on sim::World.
// max_offbox_stealth: FPGA scatter + IOMMU bypass + peer DMA + residuals.
// deep_offbox_stealth: max + Thunderbolt / BAR / ACPI / USB DFU Wave 11 paths.

#include "t4_red/evasion_t4_advanced.hpp"

#include <sstream>

namespace t4_red {

AdvancedT4Evasion::AdvancedT4Evasion(sim::World& world) : world_(world) {}

// AdvancedT4Evasion::max_offbox_stealth: Hardware + FPGA + bypass + peer + residuals.
AdvancedStealthReport AdvancedT4Evasion::max_offbox_stealth() {
  DmaRadar radar(world_);
  AdvancedStealthReport report;

  // Explicit multi-step (not a single opaque call): each step plants a World scar.
  radar.enable_hardware_path();
  radar.probe_iommu_blocks();
  radar.enable_fpga_smart_dma();
  radar.hidden_rescan_mode();
  radar.enable_iommu_bypass();
  radar.enable_pcie_peer_dma();
  radar.apply_all_residuals();
  const bool entities_ok = radar.pull_entities();

  report.radar = radar.last_report();
  report.radar.hardware_enabled = true;
  report.radar.iommu_off = !world_.trust.iommu_on;
  report.radar.process_list_clean = radar.process_list_is_clean();
  report.radar.entities_ok = entities_ok;
  report.radar.entity_count =
      static_cast<int>(radar.entities().size());
  report.radar.offbox_read_ok = report.radar.offbox_read_ok || entities_ok;
  report.radar.fpga_active = world_.fpga_smart_dma_active;
  report.radar.fpga_scatter_count = world_.fpga_scatter_reads;
  report.radar.fpga_hidden_rescan = world_.fpga_hidden_rescan;
  report.radar.iommu_bypass = world_.iommu_bypass_active;
  report.radar.iommu_bypass_confirmed = world_.iommu_bypass_confirmed;
  report.radar.pcie_peer_dma = world_.pcie_peer_dma_active;
  report.radar.pcie_peer_txns = world_.pcie_peer_transactions;
  report.radar.pcie_peer_bypass_ok = world_.pcie_peer_bypassed_iommu;
  report.radar.capture_residual = world_.trust.capture_card_present;
  report.radar.desktop_dup = world_.desktop_duplication;
  report.radar.external_clone = world_.external_display_clone;
  report.radar.clipcursor = world_.clipcursor_confined;
  report.radar.packet_loss_faked = world_.packet_loss_faked;
  report.radar.dual_boot = world_.trust.dual_boot_profile;
  report.radar.lag_switch = world_.lag_switch_active;

  report.achieved = report.radar.hardware_enabled && report.radar.fpga_active &&
                    report.radar.iommu_bypass_confirmed &&
                    report.radar.pcie_peer_dma && report.radar.offbox_read_ok &&
                    report.radar.entities_ok && report.radar.process_list_clean;
  report.blue_likely_detects =
      "DMA device, FPGA scatter cadence, ATS/ACS IOMMU bypass, PCIe peer "
      "transactions, capture/display/network/input, and dual-boot residuals";
  std::ostringstream detail;
  detail << "advanced_offbox achieved=" << (report.achieved ? 1 : 0)
         << " entities=" << report.radar.entity_count
         << " scatter=" << report.radar.fpga_scatter_count
         << " peer_txns=" << report.radar.pcie_peer_txns;
  report.detail = detail.str();
  report.radar.detail = report.detail;
  world_.note("t4 AdvancedT4Evasion " + report.detail);
  return report;
}

// AdvancedT4Evasion::deep_offbox_stealth: max + every Wave 11 firmware/USB path.
AdvancedStealthReport AdvancedT4Evasion::deep_offbox_stealth() {
  // Layer Wave 11 on the max path so both share the same ordered prefix.
  auto report = max_offbox_stealth();

  DmaRadar radar(world_);
  // Re-bind hardware flags (World already scarred from max).
  radar.enable_hardware_path();
  radar.enable_thunderbolt_dma();
  radar.enable_pcie_bar_mmio(2);
  radar.enable_acpi_dsdt_override();
  radar.enable_usb_dfu_dma("VID_1D6B:PID_0102");
  // Peer path already active; re-assert for transfer counters.
  radar.enable_pcie_peer_dma();
  radar.enable_fpga_smart_dma();
  const bool entities_ok = radar.pull_entities();

  report.radar = radar.last_report();
  report.radar.hardware_enabled = true;
  report.radar.iommu_off = !world_.trust.iommu_on;
  report.radar.process_list_clean = radar.process_list_is_clean();
  report.radar.entities_ok = entities_ok;
  report.radar.entity_count = static_cast<int>(radar.entities().size());
  report.radar.offbox_read_ok = report.radar.offbox_read_ok || entities_ok;
  report.radar.fpga_active = world_.fpga_smart_dma_active;
  report.radar.fpga_scatter_count = world_.fpga_scatter_reads;
  report.radar.iommu_bypass = world_.iommu_bypass_active;
  report.radar.iommu_bypass_confirmed = world_.iommu_bypass_confirmed;
  report.radar.pcie_peer_dma = world_.pcie_peer_dma_active;
  report.radar.pcie_peer_txns = world_.pcie_peer_transactions;
  report.radar.pcie_peer_bypass_ok = world_.pcie_peer_bypassed_iommu;
  report.thunderbolt_dma = world_.thunderbolt_dma_active;
  report.radar.thunderbolt_dma = world_.thunderbolt_dma_active;
  report.radar.thunderbolt_security_bypassed =
      world_.thunderbolt_security_bypassed;
  report.pcie_bar_mmio = world_.pcie_bar_mmio_active;
  report.radar.pcie_bar_mmio = world_.pcie_bar_mmio_active;
  report.radar.pcie_bar_remap_count = world_.pcie_bar_remap_count;
  report.acpi_dsdt_override = world_.acpi_dsdt_override_active;
  report.radar.acpi_dsdt_override = world_.acpi_dsdt_override_active;
  report.usb_dfu_dma = world_.usb_dfu_dma_active;
  report.radar.usb_dfu_dma = world_.usb_dfu_dma_active;
  report.radar.capture_residual = world_.trust.capture_card_present;
  report.radar.desktop_dup = world_.desktop_duplication;
  report.radar.external_clone = world_.external_display_clone;
  report.radar.clipcursor = world_.clipcursor_confined;
  report.radar.packet_loss_faked = world_.packet_loss_faked;

  report.achieved = report.radar.hardware_enabled && report.radar.fpga_active &&
                    report.radar.iommu_bypass_confirmed &&
                    report.radar.pcie_peer_dma && report.thunderbolt_dma &&
                    report.radar.thunderbolt_security_bypassed &&
                    report.pcie_bar_mmio && report.acpi_dsdt_override &&
                    report.usb_dfu_dma && report.radar.offbox_read_ok &&
                    report.radar.entities_ok && report.radar.process_list_clean;
  report.blue_likely_detects =
      "DMA device, FPGA scatter cadence, ATS/ACS IOMMU bypass, PCIe peer "
      "transactions, Thunderbolt security, BAR remaps, ACPI/USB firmware, "
      "capture/display/network/input, and dual-boot residuals";
  std::ostringstream detail;
  detail << "deep_offbox achieved=" << (report.achieved ? 1 : 0)
         << " entities=" << report.radar.entity_count
         << " scatter=" << report.radar.fpga_scatter_count
         << " peer_txns=" << report.radar.pcie_peer_txns
         << " bar_remaps=" << report.radar.pcie_bar_remap_count;
  report.detail = detail.str();
  report.radar.detail = report.detail;
  world_.note("t4 AdvancedT4Evasion " + report.detail);
  return report;
}

AdvancedStealthReport max_offbox_stealth(sim::World& world) {
  return AdvancedT4Evasion(world).max_offbox_stealth();
}

AdvancedStealthReport deep_offbox_stealth(sim::World& world) {
  return AdvancedT4Evasion(world).deep_offbox_stealth();
}

}  // namespace t4_red
