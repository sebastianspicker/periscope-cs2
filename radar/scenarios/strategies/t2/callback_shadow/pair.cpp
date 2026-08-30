#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `callback_shadow`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::callback_shadow::run_red(w, n);
  const auto blue = examples::callback_shadow::run_blue(w, n);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_35_callback_shadow() {
  return {{ "35_callback_shadow", "callback shadow", Family::Delivery, "T2",
           "Red multi-step lab path for callback_shadow",
           "Blue multi-reason lab path for callback_shadow"},
          run};
}
}  // namespace strategies
