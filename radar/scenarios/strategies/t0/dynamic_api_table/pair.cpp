#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::dynamic_api_table::apply(w);
  const auto blue = examples::dynamic_api_table::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_133_dynamic_api_table() {
  return {{"133_dynamic_api_table", "PEB+EAT dynamic API resolution", Family::Evasion, "T0",
           "Red resolves APIs dynamically via PEB+EAT walk",
           "Blue detects dynamic import resolution and IAT/EAT hooks"}, run};
}
}  // namespace strategies
