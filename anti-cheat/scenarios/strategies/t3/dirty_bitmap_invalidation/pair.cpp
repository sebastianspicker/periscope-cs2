#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T3 structural pair. Red's read bursts force broadcast (global) invalidation
// storms in the validator, and red observes the storm cadence to infer blue's
// scan schedule; blue detects the leak and mitigates by adopting dirty-bitmap
// selective invalidation - only flagged consumers flush.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::dirty_bitmap_invalidation::apply(w);
  const auto blue = examples::dirty_bitmap_invalidation::detect(w);
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

StrategyEntry entry_162_dirty_bitmap_invalidation() {
  return {{"162_dirty_bitmap_invalidation", "dirty-bitmap invalidation vs storm intel", Family::Structural,
           "T3",
           "Red observes broadcast invalidation storms to infer blue's scan cadence",
           "Blue detects the storm leak; mitigates with dirty-bitmap selective flush"},
          run};
}
}  // namespace strategies
