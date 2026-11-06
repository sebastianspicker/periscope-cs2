#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::heavens_gate_syscall::apply(w);
  const auto blue = examples::heavens_gate_syscall::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.risk >= 0.6;
  result.summary = red.detail + " | blue signals=" +
                   std::to_string(blue.signals) + " risk=" +
                   std::to_string(blue.risk);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}

}  // namespace

StrategyEntry entry_58_heavens_gate_syscall() {
  return {{"58_heavens_gate_syscall", "heavens gate syscall", Family::Delivery, "T1",
           "Red 32->64-bit Heaven's Gate syscall bypasses ntdll",
           "Blue detects by handle + wow64 process anomaly"},
          run};
}

}  // namespace strategies
