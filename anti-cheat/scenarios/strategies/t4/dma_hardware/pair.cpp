#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `dma_hardware`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::dma_hardware::apply(w);
  const auto blue = examples::dma_hardware::detect(w);

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

StrategyEntry entry_06_dma_hardware() {
  return {{ "06_dma_hardware", "dma hardware", Family::Delivery, "T4",
           "Red multi-step lab path for dma_hardware",
           "Blue multi-reason lab path for dma_hardware"},
          run};
}
}  // namespace strategies
