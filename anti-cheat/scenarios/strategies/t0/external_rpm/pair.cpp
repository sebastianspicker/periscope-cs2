#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T0 delivery/evasion strategy pair. Red plants foreign VM_READ channel; blue correlates
// channel telemetry with that scar rather than relying on process names alone.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::external_rpm::apply(w);
  const auto blue = examples::external_rpm::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.detected && blue.signals >= 3;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_01_external_rpm() {
  return {{"01_external_rpm", "external rpm", Family::Delivery, "T0",
"Red plants foreign VM_READ channel",
"Blue correlates telemetry with foreign VM_READ channel"}, run};
}
}  // namespace strategies
