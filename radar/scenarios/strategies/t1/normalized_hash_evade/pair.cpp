#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::normalized_hash_evade::apply(w);
  const auto blue = examples::normalized_hash_evade::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_146_normalized_hash_evade() {
  return {{"146_normalized_hash_evade", "VAC normalized PE hash evasion", Family::Evasion, "T1",
           "Red normalizes PE hash to evade VAC hash-based detection",
           "Blue detects normalized PE hashes across modules"}, run};
}
}  // namespace strategies
