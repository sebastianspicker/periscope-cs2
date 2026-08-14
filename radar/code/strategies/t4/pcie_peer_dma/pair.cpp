#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  auto red = examples::pcie_peer_dma::apply(w);
  auto blue = examples::pcie_peer_dma::detect(w);
  StrategyResult r;
  r.red_achieved = red.achieved; r.blue_detected = blue.detected; r.blue_mitigated = blue.risk >= 0.90;
  r.summary = red.detail + " | blue=" + std::to_string(blue.signals) + "sig risk=" + std::to_string(blue.risk);
  n.result(r.blue_detected || r.blue_mitigated, r.summary); return r;
}
}
StrategyEntry entry_72_pcie_peer_dma() {
  return {{"72_pcie_peer_dma", "PCIe peer-to-peer DMA", Family::Delivery, "T4",
"Red uses PCIe P2P DMA between two devices, bypasses IOMMU even when enabled",
"Blue detects via ACS/ATS enforcement or platform IOMMU grouping (near impossible)"}, run};
}
}
