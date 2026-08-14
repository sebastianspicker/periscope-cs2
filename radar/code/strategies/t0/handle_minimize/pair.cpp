#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T0 delivery/evasion strategy pair. Red plants brief VM_READ handle; blue correlates
// channel telemetry with that scar rather than relying on process names alone.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::handle_minimize::apply(w);
  const auto blue = examples::handle_minimize::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.detected && blue.signals >= 3;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_17_handle_minimize() {
  return {{"17_handle_minimize", "handle minimize", Family::Delivery, "T0",
"Red plants brief VM_READ handle",
"Blue correlates telemetry with brief VM_READ handle"}, run};
}
}  // namespace strategies
