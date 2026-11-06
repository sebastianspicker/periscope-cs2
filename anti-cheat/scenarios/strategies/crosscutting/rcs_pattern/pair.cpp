#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "rcs_pattern",
         "Pitch correction tracks spray pattern almost perfectly.");
  const auto red = examples::rcs_pattern::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "pattern fit",
            "Near-zero |expected-applied| across spray samples.");
  const auto blue = examples::rcs_pattern::detect(w);
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

StrategyEntry entry_37_rcs_pattern() {
  return {{"37_rcs_pattern", "rcs pattern", Family::Feature, "all",
           "Red: spray pitch correction tracks expected recoil pattern",
           "Blue: near-perfect RCS pattern fit residual vs humanization alone"},
          run};
}
}  // namespace strategies
