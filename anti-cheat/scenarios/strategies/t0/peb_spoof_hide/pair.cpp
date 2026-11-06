#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::peb_spoof_hide::apply(w);
  const auto blue = examples::peb_spoof_hide::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_144_peb_spoof_hide() {
  return {{"144_peb_spoof_hide", "PEB BeingDebugged/NtGlobalFlag clearing", Family::Evasion, "T0",
           "Red clears PEB BeingDebugged and NtGlobalFlag",
           "Blue detects PEB debug flag anomalies"}, run};
}
}  // namespace strategies
