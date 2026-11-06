#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T0 presentation pair. Red excludes the overlay from capture; blue diffs the
// captured frame against the presented frame and treats the empty diff as the
// scar. A player-visible, capture-invisible overlay is the whole point.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::capture_exclusion_diff::apply(w);
  const auto blue = examples::capture_exclusion_diff::detect(w);
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

StrategyEntry entry_168_capture_exclusion_diff() {
  return {{"168_capture_exclusion_diff", "capture exclusion vs frame diff",
           Family::Evasion, "T0",
           "Red excludes its overlay from screen capture while the player still sees it",
           "Blue diffs captured and presented frames; an empty diff is the scar"},
          run};
}
}  // namespace strategies
