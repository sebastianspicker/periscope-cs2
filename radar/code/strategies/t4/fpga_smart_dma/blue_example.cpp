#include "blue_example.hpp"

#include <utility>

namespace examples::fpga_smart_dma {
namespace {

void add_signal(BlueResult& result, std::string reason, double weight) {
  ++result.signals;
  result.risk += weight;
  result.reasons.push_back(std::move(reason));
}

}  // namespace

BlueResult detect(sim::World& w) {
  BlueResult result;
  const bool process_list_clean = w.processes.size() == 2 &&
                                  w.proc(w.game_pid()) != nullptr &&
                                  w.proc(w.ac_pid()) != nullptr;

  if (w.fpga_smart_dma_active) {
    add_signal(result, "autonomous FPGA scatter-gather DMA is active", 0.45);
  }
  if (w.trust.dma_device_present && !w.trust.iommu_on) {
    add_signal(result, "DMA-capable PCIe device has no IOMMU containment", 0.30);
  }
  if (w.fpga_scatter_reads > 1) {
    add_signal(result, "multiple small scatter DMA reads resemble normal PCIe traffic", 0.10);
  }
  if (w.fpga_hidden_rescan) {
    add_signal(result, "hidden FPGA rescan blends into background PCIe traffic", 0.10);
  }
  if (process_list_clean) {
    add_signal(result, "no local cheat process or game-memory handle is present", 0.05);
  }

  // Multi-reason: FPGA active alone is insufficient; require >=2 independent signals.
  const bool specific = w.fpga_smart_dma_active ||
                        (w.trust.dma_device_present && !w.trust.iommu_on);
  result.detected = specific && result.signals >= 2;
  if (result.detected && result.risk < 0.85) result.risk = 0.85;
  result.mitigated = w.trust.ranked_requires_iommu && w.trust.iommu_on;
  result.detail = "fpga_smart_dma blue signals=" +
                  std::to_string(result.signals) + " risk=" +
                  std::to_string(result.risk);
  w.note(result.detail);
  return result;
}

}  // namespace examples::fpga_smart_dma
