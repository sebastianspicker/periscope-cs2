#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::vas_walk_evade_phase::apply(w);
  const auto blue = examples::vas_walk_evade_phase::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_147_vas_walk_evade_phase() {
  return {{"147_vas_walk_evade_phase", "VAS walk phase detection + page hiding", Family::Evasion, "T1",
           "Red detects VAS walk phase and hides pages accordingly",
           "Blue detects VAS walk evasion indicators"}, run};
}
}  // namespace strategies
