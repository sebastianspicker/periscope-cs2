#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T0 delivery/evasion strategy pair. Red plants DXGI Present detour; blue correlates
// channel telemetry with that scar rather than relying on process names alone.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::dxgi_present_hook::apply(w);
  const auto blue = examples::dxgi_present_hook::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected || blue.signals >= 2;
  result.blue_mitigated = blue.mitigated || (blue.detected && blue.signals >= 3);
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_65_dxgi_present_hook() {
  return {{"65_dxgi_present_hook", "dxgi present hook", Family::Delivery, "T0",
"Red plants DXGI Present detour",
"Blue correlates telemetry with DXGI Present detour"}, run};
}
}  // namespace strategies
