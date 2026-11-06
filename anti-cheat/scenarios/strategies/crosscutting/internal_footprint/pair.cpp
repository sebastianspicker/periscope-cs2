#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T0 delivery/evasion strategy pair. Red plants manual-map footprint suppression claims; blue correlates
// channel telemetry with that scar rather than relying on process names alone.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::internal_footprint::apply(w);
  const auto blue = examples::internal_footprint::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.detected && blue.signals >= 3;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_44_internal_footprint() {
  return {{"44_internal_footprint", "internal footprint", Family::Delivery, "T0",
"Red plants manual-map footprint suppression claims",
"Blue correlates telemetry with manual-map footprint suppression claims"}, run};
}
}  // namespace strategies
