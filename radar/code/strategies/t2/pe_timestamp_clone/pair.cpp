#include "strategies/framework.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  StrategyResult result;
  result.red_achieved = pe_timestamp_clone_red_apply(w, n);
  result.blue_detected = pe_timestamp_clone_blue_detect(w, n);
  result.blue_mitigated = result.blue_detected;
  result.summary = result.blue_detected
      ? "Multi-reason: cloned timestamp co-occurs with foreign module identity residual"
      : "Cloned PE timestamp passed without multi-reason residual";
  n.result(result.blue_detected, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_153_pe_timestamp_clone() {
  return {{"153_pe_timestamp_clone", "PE Timestamp Clone Evasion", Family::Evasion, "T2",
           "Clone legitimate PE timestamp onto a foreign module",
           "Verify PE timestamps plus module identity for cloned-timestamp modules"}, run};
}
}  // namespace strategies
