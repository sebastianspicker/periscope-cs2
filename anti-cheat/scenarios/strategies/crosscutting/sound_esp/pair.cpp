#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "sound_esp",
         "Enemy positions from sound/movement without LOS.");
  const auto red = examples::sound_esp::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "audio-only product",
            "position_from_sound_only multi samples + fidelity cut.");
  const auto blue = examples::sound_esp::detect(w);
  n.say(sim::Side::Blue, blue.detail);
  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = false;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_38_sound_esp() {
  return {{"38_sound_esp", "sound esp", Family::Feature, "T0",
           "Red: movement/audio intel without LOS (sound ESP)",
           "Blue: audio-only position residual + fidelity budget cut"},
          run};
}
}  // namespace strategies
