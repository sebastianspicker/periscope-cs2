#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `module_stomp`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::module_stomp::apply(w);
  const auto blue = examples::module_stomp::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.72;
  r.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(r.blue_detected, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_54_module_stomp() {
  return {{ "54_module_stomp", "module stomp", Family::Delivery, "T1",
           "Red multi-step lab path for module_stomp",
           "Blue multi-reason lab path for module_stomp"},
          run};
}
}  // namespace strategies
