#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T0 delivery/evasion strategy pair. Red plants reputable-looking reader co-residence; blue correlates
// channel telemetry with that scar rather than relying on process names alone.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::fp_allowlist_evasion::apply(w);
  const auto blue = examples::fp_allowlist_evasion::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.detected && blue.signals >= 3;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_30_fp_allowlist_evasion() {
  return {{"30_fp_allowlist_evasion", "fp allowlist evasion", Family::Evasion, "T0",
"Red plants reputable-looking reader co-residence",
"Blue correlates telemetry with reputable-looking reader co-residence"}, run};
}
}  // namespace strategies
