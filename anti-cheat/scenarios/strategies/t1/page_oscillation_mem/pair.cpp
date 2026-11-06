#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::page_oscillation_mem::apply(w);
  const auto blue = examples::page_oscillation_mem::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_145_page_oscillation_mem() {
  return {{"145_page_oscillation_mem", "code page oscillation for VAS walk", Family::Evasion, "T1",
           "Red oscillates code page permissions to evade VAS walk",
           "Blue detects page permission oscillation patterns"}, run};
}
}  // namespace strategies
