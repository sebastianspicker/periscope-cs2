#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"
#include "fps/scenario.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  fps::RoundConfig cfg;
  cfg.freeze_time = 0.f;
  fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
  sc.start_default_round();
  n.move(sim::Side::Red, "bomb_round_intel",
         "Planted CS round; product knows fuse/site/defuse progress.");
  auto red = examples::bomb_round_intel::apply_from_scenario(sc, w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "strip bomb intel",
            "Detect timer product; clear bomb scars and deny ranked.");
  auto blue = examples::bomb_round_intel::detect_with_scenario(w, sc);
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

StrategyEntry entry_34_bomb_round_intel() {
  return {{"34_bomb_round_intel", "bomb round intel", Family::Structural, "T0",
           "Red: planted/defuse timer product beyond player XY",
           "Blue: bomb intel residual; strip timer product + ranked deny"},
          run};
}
}  // namespace strategies
