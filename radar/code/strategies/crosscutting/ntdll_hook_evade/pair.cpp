#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::ntdll_hook_evade::apply(w);
  const auto blue = examples::ntdll_hook_evade::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.risk >= 0.55;
  result.summary = red.detail + " | blue=" + std::to_string(blue.signals) +
                   "sig risk=" + std::to_string(blue.risk);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}

}  // namespace

StrategyEntry entry_67_ntdll_hook_evade() {
  return {{"67_ntdll_hook_evade", "ntdll hook evade", Family::Evasion, "T1",
           "Red detects ntdll hooks and evades via direct syscall or clean copy",
           "Blue detects via handle graph (hook telemetry is lost)"},
          run};
}

}  // namespace strategies
