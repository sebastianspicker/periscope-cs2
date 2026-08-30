#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::shellcode_inject_donor::apply(w);
  const auto blue = examples::shellcode_inject_donor::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_138_shellcode_inject_donor() {
  return {{"138_shellcode_inject_donor", "obfuscated shellcode injection into donor", Family::Delivery, "T0",
           "Red injects XOR-obfuscated shellcode into a donor process",
           "Blue detects shellcode injection via foreign thread and manual mapping scars"}, run};
}
}  // namespace strategies
