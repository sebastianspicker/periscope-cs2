#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::forensic_cleanup_exit::apply(w);
  const auto blue = examples::forensic_cleanup_exit::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_143_forensic_cleanup_exit() {
  return {{"143_forensic_cleanup_exit", "prefetch/registry/recent cleanup on exit", Family::Evasion, "T0",
           "Red cleans up forensic artifacts on exit",
           "Blue detects forensic cleanup activity"}, run};
}
}  // namespace strategies
