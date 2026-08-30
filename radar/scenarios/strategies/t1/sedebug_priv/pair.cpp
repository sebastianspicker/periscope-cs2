#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `sedebug_priv`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::sedebug_priv::apply(w);
  const auto blue = examples::sedebug_priv::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.72;
  r.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(r.blue_detected, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_90_sedebug_priv() {
  return {{ "90_sedebug_priv", "sedebug priv", Family::Delivery, "T1",
           "Red multi-step lab path for sedebug_priv",
           "Blue multi-reason lab path for sedebug_priv"},
          run};
}
}  // namespace strategies
