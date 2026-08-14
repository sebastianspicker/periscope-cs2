#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "iommu_policy",
         "DMA device with IOMMU off for off-box reads.");
  const auto red = examples::iommu_policy::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "ranked IOMMU",
            "Require IOMMU; deny ranked when DMA residual present.");
  const auto blue = examples::iommu_policy::detect(w);
  n.say(sim::Side::Blue, blue.detail);
  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.65;
  r.summary = red.detail + " | " + blue.detail + " | signals=" +
              std::to_string(blue.signals) + " risk=" + std::to_string(blue.risk);
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_56_iommu_policy() {
  return {{"56_iommu_policy", "iommu policy", Family::Delivery, "T4",
           "Red: DMA residual with IOMMU disabled",
           "Blue: ranked IOMMU policy restore + session deny"},
          run};
}

StrategyEntry entry_61_iommu_policy() { return entry_56_iommu_policy(); }

}  // namespace strategies
