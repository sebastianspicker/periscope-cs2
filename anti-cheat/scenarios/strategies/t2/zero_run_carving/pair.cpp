#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T2 evasion pair. Red verifies zero pages inside the target image and places
// code there without any allocation API, leaving the placement unreserved in
// the VAD manager; blue runs a VAD-vs-PTE consistency audit to close the gap.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::zero_run_carving::apply(w);
  const auto blue = examples::zero_run_carving::detect(w);
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

StrategyEntry entry_160_zero_run_carving() {
  return {{"160_zero_run_carving", "zero-run carving vs VAD audit", Family::Evasion,
           "T2",
           "Red places code in verified zero pages of the target without allocation APIs",
           "Blue audits VAD-vs-PTE consistency: placement not reserved in the VAD manager"},
          run};
}
}  // namespace strategies
