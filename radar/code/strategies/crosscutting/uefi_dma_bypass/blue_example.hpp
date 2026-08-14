#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::uefi_dma_bypass {

struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  std::vector<std::string> reasons;
  bool iommu_state_changed = false;
  bool acs_cleared_detected = false;
  bool pcie_anomaly = false;
  int detection_count = 0;  // alias of signals for pair.cpp compatibility
  std::string detail;
};

class Blue {
public:
  BlueResult detect(const sim::World& w) noexcept;
  BlueResult mitigate(sim::World& w) noexcept;
  static constexpr const char* kDescription =
      "Detect IOMMU bypass via DMAR table integrity check, "
      "ACS state polling on PCIe root ports, and PCIe bus "
      "traffic analysis for unexpected DMA transfers.";
};

}  // namespace examples::uefi_dma_bypass
