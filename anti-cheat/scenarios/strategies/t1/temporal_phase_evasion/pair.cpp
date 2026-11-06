#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::temporal_phase_evasion::apply(w);
  const auto blue = examples::temporal_phase_evasion::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_131_temporal_phase_evasion() {
  return {{"131_temporal_phase_evasion", "4-phase temporal jitter engine", Family::Evasion, "T1",
           "Red uses 4-phase temporal jitter engine to evade temporal detection",
           "Blue detects temporal phase transitions and jitter"}, run};
}
}  // namespace strategies
