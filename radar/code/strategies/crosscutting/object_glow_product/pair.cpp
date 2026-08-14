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
  n.move(sim::Side::Red, "object_glow_product",
         "Dropped bomb / kit / hostage glow — not fuse timer.");
  auto red = examples::object_glow_product::apply_from_scenario(sc, w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "object-class strip",
            "Clear glow product; fog object replication.");
  auto blue = examples::object_glow_product::detect(w);
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

StrategyEntry entry_42_object_glow_product() {
  return {{"42_object_glow_product", "object glow product", Family::Structural, "T0",
           "Red: dropped bomb/kit/hostage glow product (not fuse timer)",
           "Blue: object-class glow residual; strip + fidelity fog"},
          run};
}
}  // namespace strategies
