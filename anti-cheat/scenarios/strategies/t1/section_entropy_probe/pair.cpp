#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T1 section-shape pair. Red ships an encrypted registry and decodes into
// registers, so the data-section-patch heuristic sees nothing. Blue measures
// non-image section entropy instead and demands a manifest.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::section_entropy_probe::apply(w);
  const auto blue = examples::section_entropy_probe::detect(w);
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

StrategyEntry entry_166_section_entropy_probe() {
  return {{"166_section_entropy_probe", "high-entropy section vs patch-free decode",
           Family::Evasion, "T1",
           "Red decodes an encrypted registry into registers, never patching the data section",
           "Blue measures non-image section entropy and requires a manifest"},
          run};
}
}  // namespace strategies
