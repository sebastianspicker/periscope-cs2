#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::ept_memory_hiding::apply(w);
  const auto blue = examples::ept_memory_hiding::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_136_ept_memory_hiding() {
  return {{"136_ept_memory_hiding", "EPT-based physical memory hiding", Family::Evasion, "T3",
           "Red hides physical memory pages via EPT manipulation",
           "Blue detects EPT memory hiding via multiple indicators"}, run};
}
}  // namespace strategies
