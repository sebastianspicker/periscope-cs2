#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `skin_changer`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::skin_changer::apply(w);
  const auto blue = examples::skin_changer::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = false;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_100_skin_changer() {
  return {{"100_skin_changer", "skin changer", Family::Feature, "all",
           "Red multi-step lab path for skin_changer",
           "Blue multi-reason lab path for skin_changer"},
          run};
}
}  // namespace strategies
