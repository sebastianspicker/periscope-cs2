#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::hw_monitor_disguise::apply(w);
  const auto blue = examples::hw_monitor_disguise::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_142_hw_monitor_disguise() {
  return {{"142_hw_monitor_disguise", "RTSS/Afterburner decoy OSD display", Family::Evasion, "T0",
           "Red disguises overlay as RTSS/Afterburner OSD display",
           "Blue detects HW monitor disguise via process + overlay analysis"}, run};
}
}  // namespace strategies
