#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "no_flash", "Strip client flash FX alpha to zero.");
  const auto red = examples::no_flash::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "fx strip + helper",
            "Presentation residual with inject helper multi-reason.");
  const auto blue = examples::no_flash::detect(w);
  n.say(sim::Side::Blue, blue.detail);
  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_40_no_flash() {
  return {{"40_no_flash", "no flash", Family::Feature, "T0",
           "Red: client flash FX strip (alpha forced to zero)",
           "Blue: FX strip residual + helper module multi-reason"},
          run};
}
}  // namespace strategies
