#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::decoy_render_ml_evasion::apply(w);
  const auto blue = examples::decoy_render_ml_evasion::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_132_decoy_render_ml_evasion() {
  return {{"132_decoy_render_ml_evasion", "ML behavioral confusion via decoy render", Family::Evasion, "T1",
           "Red uses decoy rendering to confuse ML behavioral detection",
           "Blue detects ML confusion engine and decoy render patterns"}, run};
}
}  // namespace strategies
