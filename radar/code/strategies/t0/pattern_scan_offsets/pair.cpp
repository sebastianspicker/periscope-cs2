#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::pattern_scan_offsets::apply(w);
  const auto blue = examples::pattern_scan_offsets::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_129_pattern_scan_offsets() {
  return {{"129_pattern_scan_offsets", "SIMD pattern scanner with RIP resolution", Family::Delivery, "T0",
           "Red uses SIMD pattern scanning with RIP-relative offset resolution",
           "Blue detects bulk reads and pattern marker presence"}, run};
}
}  // namespace strategies
