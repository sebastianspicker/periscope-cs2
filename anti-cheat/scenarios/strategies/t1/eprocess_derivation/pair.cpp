#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T1 evasion pair. Red derives structural offsets at runtime from
// self-validating invariants (list cycle, DTB probe, PEB Ldr cycle) and
// re-derives after layout churn, shipping no offsets; blue scores the probe
// read-scar pattern and the version-churn pressure, then re-baselines.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::eprocess_derivation::apply(w);
  const auto blue = examples::eprocess_derivation::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals) +
                   " risk=" + std::to_string(blue.risk);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_156_eprocess_derivation() {
  return {{"156_eprocess_derivation", "runtime structural derivation", Family::Evasion,
           "T0-T1",
           "Red derives structure offsets from self-validating invariants, no shipped offsets",
           "Blue detects the derivation probe read-scar pattern and churn pressure"},
          run};
}
}  // namespace strategies
