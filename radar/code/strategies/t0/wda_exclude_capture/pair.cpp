#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::wda_exclude_capture::apply(w);
  const auto blue = examples::wda_exclude_capture::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_125_wda_exclude_capture() {
  return {{"125_wda_exclude_capture", "WDA EXCLUDEFROMCAPTURE overlay", Family::Evasion, "T0",
           "Red applies SetWindowDisplayAffinity EXCLUDEFROMCAPTURE on overlay",
           "Blue detects capture-excluded overlays"}, run};
}
}  // namespace strategies
