#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::hud_radar_parsing::apply(w);
  const auto blue = examples::hud_radar_parsing::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_127_hud_radar_parsing() {
  return {{"127_hud_radar_parsing", "CCSGO_HudRadar BST walk + snapshot", Family::Feature, "T0",
           "Red walks CCSGO_HudRadar BST to collect entity snapshots",
           "Blue detects radar BST walk via remote read telemetry"}, run};
}
}  // namespace strategies
