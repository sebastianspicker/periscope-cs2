#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T0 detection pair. Red reads entity fields in separate transactions while
// the game state advances between them, producing torn/garbled records; blue
// validates the assembled records against plausibility invariants
// (team/lifestate/origin/health combinations) and scores the read pattern.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::torn_snapshot_artifacts::apply(w);
  const auto blue = examples::torn_snapshot_artifacts::detect(w);
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

StrategyEntry entry_161_torn_snapshot_artifacts() {
  return {{"161_torn_snapshot_artifacts", "torn multi-read snapshots", Family::Detection,
           "T0",
           "Red reads entity fields in several transactions, producing torn records",
           "Blue detects implausible field combinations vs single-transaction reads"},
          run};
}
}  // namespace strategies
