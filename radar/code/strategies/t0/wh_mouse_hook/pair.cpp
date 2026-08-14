#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T0 delivery/evasion strategy pair. Red plants global mouse hook; blue correlates
// channel telemetry with that scar rather than relying on process names alone.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::wh_mouse_hook::apply(w);
  const auto blue = examples::wh_mouse_hook::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.detected && blue.signals >= 3;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_77_wh_mouse_hook() {
  return {{"77_wh_mouse_hook", "wh mouse hook", Family::Delivery, "T0",
"Red plants global mouse hook",
"Blue correlates telemetry with global mouse hook"}, run};
}
}  // namespace strategies
