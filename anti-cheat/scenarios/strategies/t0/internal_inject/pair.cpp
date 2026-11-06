#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T0 delivery/evasion strategy pair. Red plants foreign module and execution thread; blue correlates
// channel telemetry with that scar rather than relying on process names alone.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::internal_inject::apply(w);
  const auto blue = examples::internal_inject::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.detected && blue.signals >= 3;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_07_internal_inject() {
  return {{"07_internal_inject", "internal inject", Family::Delivery, "T0",
"Red plants foreign module and execution thread",
"Blue correlates telemetry with foreign module and execution thread"}, run};
}
}  // namespace strategies
