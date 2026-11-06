#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "triggerbot_timing",
         "Crosshair-on-enemy fires with superhuman latency.");
  const auto red = examples::triggerbot_timing::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "latency floor",
            "Score on-target fire latency below human floor.");
  const auto blue = examples::triggerbot_timing::detect(w);
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

StrategyEntry entry_36_triggerbot_timing() {
  return {{"36_triggerbot_timing", "triggerbot timing", Family::Feature, "all",
           "Red: fire when crosshair on enemy with superhuman latency",
           "Blue: on-target fire latency residual vs soft-aim alone"},
          run};
}
}  // namespace strategies
