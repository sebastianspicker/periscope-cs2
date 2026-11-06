#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::dxgi_output_duplication::apply(w);
  const auto blue = examples::dxgi_output_duplication::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_126_dxgi_output_duplication() {
  return {{"126_dxgi_output_duplication", "DXGI output duplication composite", Family::Feature, "T0",
           "Red uses IDXGIOutputDuplication to acquire frames and composite output",
           "Blue detects DXGI output duplication activity"}, run};
}
}  // namespace strategies
