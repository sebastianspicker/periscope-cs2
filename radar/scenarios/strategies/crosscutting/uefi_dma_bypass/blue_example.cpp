#include "blue_example.hpp"

#include <sstream>

namespace examples::uefi_dma_bypass {

BlueResult Blue::detect(const sim::World& w) noexcept {
  BlueResult result;

  // Reason 1: IOMMU trust state flipped / bypass scar.
  if (!w.trust.iommu_on || w.iommu_bypass_active || w.iommu_disabled) {
    result.iommu_state_changed = true;
    result.reasons.emplace_back("IOMMU disabled or bypass active");
  }

  // Reason 2: ACS cleared / peer path around remapping.
  if (w.acs_cleared || w.pcie_peer_bypassed_iommu) {
    result.acs_cleared_detected = true;
    result.reasons.emplace_back("ACS cleared or PCIe peer bypassed IOMMU");
  }

  // Reason 3: unexpected PCIe DMA with remapping off.
  if (w.trust.dma_device_present &&
      (w.pcie_peer_dma_active || w.dma_enabled) && !w.trust.iommu_on) {
    result.pcie_anomaly = true;
    result.reasons.emplace_back("PCIe DMA device active with IOMMU off");
  }

  // Reason 4: IOMMU state machine reports Bypassed.
  if (w.iommu_state == sim::IommuState::Bypassed || w.iommu_bypass_confirmed) {
    result.reasons.emplace_back("iommu_state Bypassed or bypass confirmed");
  }

  // Reason 5: DMAR DRHD residual (table analysis footprint).
  if (w.dmar_drhd_count > 0 && w.iommu_bypass_active) {
    result.reasons.emplace_back("DMAR DRHD count with active bypass scar");
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.detection_count = result.signals;
  result.detected = result.signals >= 2;

  std::ostringstream oss;
  if (result.signals == 0) {
    oss << "IOMMU state nominal";
  } else {
    oss << "uefi_dma_bypass blue signals=" << result.signals;
    for (const auto& reason : result.reasons) {
      oss << " | " << reason;
    }
  }
  result.detail = oss.str();
  return result;
}

BlueResult Blue::mitigate(sim::World& w) noexcept {
  auto result = detect(w);
  if (result.detected) {
    w.note("Blue: IOMMU bypass multi-signal detect");
    w.note("Blue: remediation: request firmware re-enable VT-d");
    w.trust.iommu_on = true;
    w.iommu_bypass_active = false;
    w.iommu_bypass_confirmed = false;
    w.iommu_disabled = false;
    w.iommu_state = sim::IommuState::Enabled;
    w.acs_cleared = false;
    w.ranked_access_denied = true;
    result.mitigated = true;
    result.detail += " | mitigated";
  }
  return result;
}

}  // namespace examples::uefi_dma_bypass
