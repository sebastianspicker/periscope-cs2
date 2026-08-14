#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "49_crosshair_helper", "Red: crosshair / sniper-crosshair / recoil-crosshair UI assist");
  const auto red = examples::crosshair_helper::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "49_crosshair_helper", "Blue: aim-UI residual distinct from soft-aim and triggerbot");
  const auto blue = examples::crosshair_helper::detect(w);
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

StrategyEntry entry_49_crosshair_helper() {
  return {{ "49_crosshair_helper", "crosshair helper", Family::Feature, "all",
           "Red: crosshair / sniper-crosshair / recoil-crosshair UI assist",
           "Blue: aim-UI residual distinct from soft-aim and triggerbot"},
          run};
}
}  // namespace strategies
