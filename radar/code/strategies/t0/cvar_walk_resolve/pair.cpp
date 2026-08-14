#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::cvar_walk_resolve::apply(w);
  const auto blue = examples::cvar_walk_resolve::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_128_cvar_walk_resolve() {
  return {{"128_cvar_walk_resolve", "4-tier CVar resolution cascade", Family::Feature, "T0",
           "Red resolves CVars through 4-tier cascade",
           "Blue detects CVar walk via remote read telemetry"}, run};
}
}  // namespace strategies
