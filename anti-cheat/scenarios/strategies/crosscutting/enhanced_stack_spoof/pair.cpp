#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::enhanced_stack_spoof::apply(w);
  const auto blue = examples::enhanced_stack_spoof::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.risk >= 0.55;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals) + " depth=" + std::to_string(red.steps > 0 ? 4 : 0);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}
StrategyEntry entry_59_enhanced_stack_spoof() {
  return {{"59_enhanced_stack_spoof", "enhanced stack spoof", Family::Evasion, "T1",
"Red forges multi-frame call stack to defeat attribution",
"Blue detects spoofed stack but struggles with origin attribution"}, run};
}
}
