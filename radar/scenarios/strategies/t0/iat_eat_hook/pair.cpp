#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T0 delivery/evasion strategy pair. Red plants IAT and EAT detours; blue correlates
// channel telemetry with that scar rather than relying on process names alone.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::iat_eat_hook::apply(w);
  const auto blue = examples::iat_eat_hook::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.detected && blue.signals >= 3;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_64_iat_eat_hook() {
  return {{"64_iat_eat_hook", "iat eat hook", Family::Delivery, "T0",
"Red plants IAT and EAT detours",
"Blue correlates telemetry with IAT and EAT detours"}, run};
}
}  // namespace strategies
