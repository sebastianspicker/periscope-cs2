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
  n.move(sim::Side::Red, "plant_defuse_hud",
         "Plant-feasibility + defuse-window HUD (not fuse timer).");
  auto red = examples::plant_defuse_hud::apply_from_scenario(sc, w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "strip HUD alerts",
            "Clear plant/defuse window scars.");
  auto blue = examples::plant_defuse_hud::detect(w);
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

StrategyEntry entry_43_plant_defuse_hud() {
  return {{"43_plant_defuse_hud", "plant defuse hud", Family::Structural, "T0",
           "Red: plant-feasibility / defuse-window HUD timing alerts",
           "Blue: HUD alert residual distinct from fuse timer product"},
          run};
}
}  // namespace strategies
