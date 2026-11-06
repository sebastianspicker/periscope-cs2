#include "red_example.hpp"

namespace examples::uefi_dma_bypass {

void Red::apply(sim::World& w) noexcept {
  (void)apply_detailed(w);
}

RedResult Red::apply_detailed(sim::World& w) noexcept {
  RedResult r;

  // Phase 1: analyze DMAR table residual (UEFI ACPI story).
  w.dmar_drhd_count = 2;
  r.dmar_analyzed = true;
  w.note("Red: reading DMAR table from UEFI ACPI");
  w.note("Red: scanning DMA remapping units");
  ++r.steps;

  // Phase 2: clear ACS on upstream PCIe root ports.
  w.pcie_peer_bypassed_iommu = true;
  w.acs_cleared = true;
  r.acs_cleared = true;
  w.note("Red: cleared ACS Source Validation on root port");
  ++r.steps;

  // Phase 3: disable IOMMU remapping / trust posture scars.
  w.trust.iommu_on = false;
  w.iommu_bypass_active = true;
  w.iommu_bypass_confirmed = true;
  w.iommu_disabled = true;
  w.iommu_state = sim::IommuState::Bypassed;
  r.iommu_bypass_active = true;
  ++r.steps;

  // Phase 4: PCIe DMA device present with peer DMA active.
  w.trust.dma_device_present = true;
  w.pcie_peer_dma_active = true;
  w.pcie_peer_transactions = 8;
  w.dma_enabled = true;
  r.pcie_dma_possible = true;
  ++r.steps;

  r.achieved = r.steps >= 2 && w.iommu_bypass_active && !w.trust.iommu_on &&
               w.trust.dma_device_present;
  r.detail = "uefi_dma_bypass red steps=" + std::to_string(r.steps) +
             " iommu_off=1 acs_cleared=1 dma_device=1";
  w.note(r.detail);
  w.note("Red: IOMMU bypass complete, DMA available");
  return r;
}

}  // namespace examples::uefi_dma_bypass
