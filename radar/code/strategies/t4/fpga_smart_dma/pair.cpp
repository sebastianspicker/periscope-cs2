#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::fpga_smart_dma::apply(w);
  const auto blue = examples::fpga_smart_dma::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.risk >= 0.85;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals) + " risk=" + std::to_string(blue.risk);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}
StrategyEntry entry_63_fpga_smart_dma() {
  return {{"63_fpga_smart_dma", "fpga smart dma", Family::Delivery, "T4",
"Red FPGA scatter-gather DMA reads entities without OS scars",
"Blue detects via IOMMU policy or PCIe traffic analysis (extremely hard)"}, run};
}
}
