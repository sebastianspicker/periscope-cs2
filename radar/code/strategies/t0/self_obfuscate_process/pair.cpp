#include "strategies/framework.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  StrategyResult result;
  result.red_achieved = self_obfuscate_process_red_apply(w, n);
  result.blue_detected = self_obfuscate_process_blue_detect(w, n);
  result.blue_mitigated = result.blue_detected;
  result.summary = result.blue_detected
      ? "Multi-reason: identity façade co-occurs with game-access residual"
      : "No multi-reason process identity/access residual was found";
  n.result(result.blue_detected, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_113_self_obfuscate_process() {
  return {{"113_self_obfuscate_process", "Process Self-Obfuscation", Family::Evasion, "T0-T1",
           "Apply a simulated RTSS process identity profile",
           "Validate the claimed process identity and its game access"}, run};
}
}  // namespace strategies
