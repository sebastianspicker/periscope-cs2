#pragma once

// T4 advanced off-box evasion narrative. All actions mutate sim::World only.

#include "t4_red/dma_radar.hpp"

#include <string>

namespace t4_red {

struct AdvancedStealthReport {
  DmaRadarReport radar;
  bool achieved = false;
  bool thunderbolt_dma = false;
  bool pcie_bar_mmio = false;
  bool acpi_dsdt_override = false;
  bool usb_dfu_dma = false;
  std::string blue_likely_detects;
  std::string detail;
};

/// Coordinates every simulated T4 DMA and residual path for lab scenarios.
class AdvancedT4Evasion {
 public:
  explicit AdvancedT4Evasion(sim::World& world);

  /// Chain hardware DMA, FPGA scatter, IOMMU bypass, peer DMA, and residuals.
  AdvancedStealthReport max_offbox_stealth();

  /// Chain every simulated T4 DMA, firmware, USB, and residual technique.
  AdvancedStealthReport deep_offbox_stealth();

 private:
  sim::World& world_;
};

using EvasionT4Advanced = AdvancedT4Evasion;

/// Free-function form for strategy wrappers.
AdvancedStealthReport max_offbox_stealth(sim::World& world);
AdvancedStealthReport deep_offbox_stealth(sim::World& world);

}  // namespace t4_red
