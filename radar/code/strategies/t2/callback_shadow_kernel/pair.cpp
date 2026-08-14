#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::callback_shadow_kernel::apply(w);
  const auto blue = examples::callback_shadow_kernel::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_135_callback_shadow_kernel() {
  return {{"135_callback_shadow_kernel", "kernel callback strip for BYOVD", Family::Evasion, "T2",
           "Red strips kernel callbacks during BYOVD IOCTL",
           "Blue detects callback count mismatch"}, run};
}
}  // namespace strategies
