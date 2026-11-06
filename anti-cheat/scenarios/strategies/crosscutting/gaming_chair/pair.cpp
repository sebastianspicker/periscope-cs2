#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `gaming_chair`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::gaming_chair::apply(w);
  const auto blue = examples::gaming_chair::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_101_gaming_chair() {
  return {{"101_gaming_chair", "gaming chair product suite", Family::Feature, "all",
           "Red multi-step lab path for gaming_chair",
           "Blue multi-reason lab path for gaming_chair"},
          run};
}
}  // namespace strategies
