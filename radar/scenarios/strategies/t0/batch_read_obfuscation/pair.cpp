#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::batch_read_obfuscation::apply(w);
  const auto blue = examples::batch_read_obfuscation::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_130_batch_read_obfuscation() {
  return {{"130_batch_read_obfuscation", "batch reads with Fisher-Yates shuffle + jitter", Family::Evasion, "T0",
           "Red performs batch reads with Fisher-Yates shuffle and timing jitter",
           "Blue detects scattered read pattern and jitter telemetry"}, run};
}
}  // namespace strategies
