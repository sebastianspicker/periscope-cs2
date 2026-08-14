#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::accept_readiness_gate::apply(w);
  const auto blue = examples::accept_readiness_gate::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_141_accept_readiness_gate() {
  return {{"141_accept_readiness_gate", "7-condition session readiness gate", Family::Structural, "T0",
           "Red bypasses 7-condition readiness gate",
           "Blue detects readiness gate bypass"}, run};
}
}  // namespace strategies
