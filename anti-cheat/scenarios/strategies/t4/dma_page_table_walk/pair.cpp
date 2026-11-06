#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::dma_page_table_walk::apply(w);
  const auto blue = examples::dma_page_table_walk::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_137_dma_page_table_walk() {
  return {{"137_dma_page_table_walk", "physical memory page table walk via DMA", Family::Delivery, "T4",
           "Red walks physical memory page tables via DMA device",
           "Blue detects DMA page table walk activity"}, run};
}
}  // namespace strategies
