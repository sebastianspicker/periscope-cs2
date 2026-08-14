#pragma once

// T4 educational DMA residual — sim. 
// Off-box path: marks dma_device_present, forces iommu off, dma_read without
// a local reader process or game handle. Process list stays clean.

#include "ac/types.hpp"
#include "sim/world.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace t4_red {

// Aggregate outcome fields for `RedResult` (lab narrative / tests).
struct RedResult {
  bool achieved = false;            // off-box advantage established
  bool process_list_clean = false;  // no local reader / no foreign VM_READ
  bool offbox_read_ok = false;      // dma_read succeeded without process
  std::size_t entities = 0;
  std::string detail;
};

// Aggregate outcome fields for `DmaRadarReport` (lab narrative / tests).
struct DmaRadarReport {
  bool hardware_enabled = false;
  bool iommu_off = false;
  bool process_list_clean = false;
  bool offbox_read_ok = false;
  bool entities_ok = false;
  int entity_count = 0;
  int dma_ops = 0;
  std::uint64_t bytes_read = 0;
  bool capture_residual = false;
  bool desktop_dup = false;
  bool external_clone = false;
  bool lag_switch = false;
  bool dual_boot = false;
  bool clipcursor = false;
  bool packet_loss_faked = false;
  bool iommu_blocked_probe = false;  // confirmed dma_read fails with iommu on
  bool fpga_active = false;
  int fpga_scatter_count = 0;
  bool fpga_hidden_rescan = false;
  bool iommu_bypass = false;
  bool iommu_bypass_confirmed = false;
  bool pcie_peer_dma = false;
  int pcie_peer_txns = 0;
  bool pcie_peer_bypass_ok = false;
  bool thunderbolt_dma = false;
  bool thunderbolt_security_bypassed = false;
  bool pcie_bar_mmio = false;
  int pcie_bar_remap_count = 0;
  bool acpi_dsdt_override = false;
  bool usb_dfu_dma = false;
  std::string detail;
};

/// Simulated second-machine / FPGA radar: marks DMA present, weak IOMMU,
/// never opens a process handle to the game. sim — 
class DmaRadar {
 public:
  explicit DmaRadar(sim::World& world);

  /// Enable residual hardware path (dma_device_present, iommu off).
  void enable_hardware_path();

  /// Temporarily probe: with iommu on, dma_read must fail; restore iommu_off.
  bool probe_iommu_blocks();

  /// Enable simulated FPGA scatter-gather reads; no physical device is touched.
  void enable_fpga_smart_dma();

  /// Read a range page-by-page through the simulated FPGA DMA path.
  bool run_scatter_read(std::uint32_t target_pid, std::uint64_t address,
                        std::size_t size, std::vector<std::uint8_t>& out);

  /// Mark the simulated FPGA's low-rate hidden re-scan behavior.
  void hidden_rescan_mode();

  /// Enable the simulated ATS/ACS IOMMU-bypass scar without changing IOMMU policy.
  void enable_iommu_bypass();

  /// Enable the simulated two-device PCIe peer-to-peer exfiltration path.
  void enable_pcie_peer_dma();

  /// Simulate a Thunderbolt PCIe hotplug DMA path and security bypass.
  void enable_thunderbolt_dma();

  /// Simulate mapping physical memory into PCIe BAR MMIO space.
  void enable_pcie_bar_mmio(int count);

  /// Simulate an ACPI DSDT override for firmware persistence.
  void enable_acpi_dsdt_override();

  /// Simulate DMA through a USB Device Firmware Upgrade interface.
  void enable_usb_dfu_dma(
      const std::string& device_id = "VID_1D6B:PID_0102");

  /// Pull entity bytes via World::dma_read (no reader pid / no handle).
  /// Plants lab entities if game memory is thin.
  bool pull_entities();

  /// Plant residual multi-step scars (capture card, DXGI, clone, lag, dual boot).
  void apply_residuals(bool capture = true, bool desktop = true,
                       bool clone = true, bool lag = false,
                       bool dual_boot = false);

  /// Plant every T4 residual scar represented by the World model.
  void apply_all_residuals();

  /// Full loop: enable → optional iommu-block probe → pull → residuals.
  DmaRadarReport run_full_loop(bool residuals = true);

  /// Full simulated off-box path: DMA, FPGA, IOMMU bypass, peer DMA, residuals, read.
  DmaRadarReport run_full_stealth_loop();

  /// Full red step for strategy / duel glue: enable + pull + clean check.
  RedResult apply();

  bool hardware_enabled() const;
  /// True when no non-game non-ac process holds VM_READ on the game.
  bool process_list_is_clean() const;
  const std::vector<ac::EntitySnapshot>& entities() const { return entities_; }
  const DmaRadarReport& last_report() const { return last_; }

 private:
  sim::World& world_;
  bool enabled_ = false;
  int dma_ops_ = 0;
  std::uint64_t bytes_read_ = 0;
  std::vector<ac::EntitySnapshot> entities_;
  DmaRadarReport last_{};
};

/// Free-function form used by strategy wrappers.
RedResult apply(sim::World& world);

}  // namespace t4_red
