#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `overlay_esp`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::overlay_esp::apply(w);
  const auto blue = examples::overlay_esp::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_09_overlay_esp() {
  return {{ "09_overlay_esp", "overlay esp", Family::Feature, "all",
           "Red multi-step lab path for overlay_esp",
           "Blue multi-reason lab path for overlay_esp"},
          run};
}
}  // namespace strategies
