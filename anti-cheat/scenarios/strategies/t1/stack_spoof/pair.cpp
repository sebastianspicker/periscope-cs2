#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `stack_spoof`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::stack_spoof::apply(w);
  const auto blue = examples::stack_spoof::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.72;
  r.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(r.blue_detected, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_44_stack_spoof() {
  return {{ "44_stack_spoof", "stack spoof", Family::Delivery, "T1",
           "Red multi-step lab path for stack_spoof",
           "Blue multi-reason lab path for stack_spoof"},
          run};
}
}  // namespace strategies
