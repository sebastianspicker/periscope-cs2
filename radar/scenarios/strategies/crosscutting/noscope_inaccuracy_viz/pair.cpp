#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "47_noscope_inaccuracy_viz", "Red: sniper no-scope inaccuracy/spread visualization product");
  const auto red = examples::noscope_inaccuracy_viz::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "47_noscope_inaccuracy_viz", "Blue: combat presentation residual (niche)");
  const auto blue = examples::noscope_inaccuracy_viz::detect(w);
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

StrategyEntry entry_47_noscope_inaccuracy_viz() {
  return {{ "47_noscope_inaccuracy_viz", "noscope inaccuracy viz", Family::Feature, "T0",
           "Red: sniper no-scope inaccuracy/spread visualization product",
           "Blue: combat presentation residual (niche)"},
          run};
}
}  // namespace strategies
