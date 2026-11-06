#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `early_load_race`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::early_load_race::run_red(w, n);
  const auto blue = examples::early_load_race::run_blue(w, n);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_46_early_load_race() {
  return {{ "46_early_load_race", "early load race", Family::Delivery, "T2",
           "Red multi-step lab path for early_load_race",
           "Blue multi-reason lab path for early_load_race"},
          run};
}
}  // namespace strategies
