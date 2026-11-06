#pragma once
#include "sim/world.hpp"
#include <cstdint>
#include <string>

namespace examples::uefi_dma_bypass {

struct RedResult {
  bool achieved = false;
  bool dmar_analyzed = false;
  bool iommu_bypass_active = false;
  bool acs_cleared = false;
  bool pcie_dma_possible = false;
  int steps = 0;
  std::string detail;
};

class Red {
public:
  void apply(sim::World& w) noexcept;
  RedResult apply_detailed(sim::World& w) noexcept;
  static constexpr const char* kDescription =
      "Bypass DMA remapping (IOMMU/VT-d) using UEFI firmware table "
      "analysis: read DMAR table, clear ACS on root ports, disable "
      "remapping. Opens PCIe DMA for FPGA-based memory reads.";
};

}  // namespace examples::uefi_dma_bypass
