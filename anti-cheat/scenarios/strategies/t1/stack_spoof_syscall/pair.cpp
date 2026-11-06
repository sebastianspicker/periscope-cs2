#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::stack_spoof_syscall::apply(w);
  const auto blue = examples::stack_spoof_syscall::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_134_stack_spoof_syscall() {
  return {{"134_stack_spoof_syscall", "return address spoofing for syscalls", Family::Evasion, "T1",
           "Red spoofs return addresses for syscall path",
           "Blue detects stack spoofing and syscall path anomalies"}, run};
}
}  // namespace strategies
