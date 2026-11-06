#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::runtime_health_ladder::apply(w);
  const auto blue = examples::runtime_health_ladder::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_140_runtime_health_ladder() {
  return {{"140_runtime_health_ladder", "3-level healing ladder", Family::Structural, "T0",
           "Red maintains 3-level runtime health ladder with self-heal",
           "Blue detects health ladder state transitions"}, run};
}
}  // namespace strategies
