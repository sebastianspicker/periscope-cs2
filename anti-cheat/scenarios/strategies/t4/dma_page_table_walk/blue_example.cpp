#include "blue_example.hpp"
#include <cstdio>

namespace examples::dma_page_table_walk {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.dma_page_table_walked) {
    result.reasons.emplace_back("DMA page table walk detected");
    result.dma_walk_detected = true;
  }
  if (w.dma_page_entries_found > 0) result.reasons.emplace_back("DMA page entries found=" + std::to_string(w.dma_page_entries_found));
  if (w.dma_page_levels_walked > 0) result.reasons.emplace_back("DMA page levels walked=" + std::to_string(w.dma_page_levels_walked));
  if (w.pcie_peer_dma_active) result.reasons.emplace_back("PCIe peer DMA active, transactions=" + std::to_string(w.pcie_peer_transactions));
  if (w.trust.dma_device_present && !w.trust.iommu_on) result.reasons.emplace_back("DMA device present but IOMMU disabled");

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.dma_walk_detected;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T4 dma_page_table_walk] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::dma_page_table_walk
