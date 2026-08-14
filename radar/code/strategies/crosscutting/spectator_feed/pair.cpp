#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "spectator_feed",
         "Observer list + delayed enemy origin product.");
  const auto red = examples::spectator_feed::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "cut delayed spectator intel",
            "Feed + delayed origin residual → clear scars.");
  const auto blue = examples::spectator_feed::detect(w);
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

StrategyEntry entry_35_spectator_feed() {
  return {{"35_spectator_feed", "spectator feed", Family::Structural, "T0",
           "Red: spectator list + delayed enemy origin without local LOS",
           "Blue: spectator feed residual; strip delayed origin + ranked deny"},
          run};
}
}  // namespace strategies
