#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "projectile_nade_esp",
         "Projectile ESP + nade arc prediction product.");
  const auto red = examples::projectile_nade_esp::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "object-class fog",
            "Non-player projectile product + prediction residual.");
  const auto blue = examples::projectile_nade_esp::detect(w);
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

StrategyEntry entry_41_projectile_nade_esp() {
  return {{"41_projectile_nade_esp", "projectile nade esp", Family::Feature, "T0",
           "Red: grenade/projectile ESP + nade prediction arcs",
           "Blue: object-class product residual + fidelity fog"},
          run};
}
}  // namespace strategies
