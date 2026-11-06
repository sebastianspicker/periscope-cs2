#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "46_fov_viewmodel_mod", "Red: client FOV + viewmodel FOV presentation overrides");
  const auto red = examples::fov_viewmodel_mod::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "46_fov_viewmodel_mod", "Blue: presentation residual multi-reason (weak alone + ranked policy)");
  const auto blue = examples::fov_viewmodel_mod::detect(w);
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

StrategyEntry entry_46_fov_viewmodel_mod() {
  return {{ "46_fov_viewmodel_mod", "fov viewmodel mod", Family::Feature, "T0",
           "Red: client FOV + viewmodel FOV presentation overrides",
           "Blue: presentation residual multi-reason (weak alone + ranked policy)"},
          run};
}
}  // namespace strategies
