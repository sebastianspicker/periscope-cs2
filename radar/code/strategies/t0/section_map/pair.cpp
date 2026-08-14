#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T0 delivery/evasion strategy pair. Red plants shared entity-data section; blue correlates
// channel telemetry with that scar rather than relying on process names alone.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::section_map::apply(w);
  const auto blue = examples::section_map::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.detected && blue.signals >= 3;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_43_section_map() {
  return {{"43_section_map", "section map", Family::Delivery, "T0",
"Red plants shared entity-data section",
"Blue correlates telemetry with shared entity-data section"}, run};
}
}  // namespace strategies
