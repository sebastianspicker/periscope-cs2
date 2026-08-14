#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T0 delivery/evasion strategy pair. Red plants co-resident radar process; blue correlates
// channel telemetry with that scar rather than relying on process names alone.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::process_cooccurrence::apply(w);
  const auto blue = examples::process_cooccurrence::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.detected && blue.signals >= 3;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_29_process_cooccurrence() {
  return {{"29_process_cooccurrence", "process cooccurrence", Family::Delivery, "T0",
"Red plants co-resident radar process",
"Blue correlates telemetry with co-resident radar process"}, run};
}
}  // namespace strategies
