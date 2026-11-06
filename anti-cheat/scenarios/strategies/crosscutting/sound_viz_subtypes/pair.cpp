#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "48_sound_viz_subtypes", "Red: sound visualizer subtypes (reload/scope/bomb-beep/footstep)");
  const auto red = examples::sound_viz_subtypes::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "48_sound_viz_subtypes", "Blue: multi-subtype product residual finer than generic sound ESP 38");
  const auto blue = examples::sound_viz_subtypes::detect(w);
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

StrategyEntry entry_48_sound_viz_subtypes() {
  return {{ "48_sound_viz_subtypes", "sound viz subtypes", Family::Feature, "T0",
           "Red: sound visualizer subtypes (reload/scope/bomb-beep/footstep)",
           "Blue: multi-subtype product residual finer than generic sound ESP 38"},
          run};
}
}  // namespace strategies
