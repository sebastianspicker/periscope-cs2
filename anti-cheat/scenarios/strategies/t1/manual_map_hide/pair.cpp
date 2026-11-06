#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `manual_map_hide`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::manual_map_hide::apply(w);
  const auto blue = examples::manual_map_hide::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.72;
  r.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(r.blue_detected, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_08_manual_map_hide() {
  return {{ "08_manual_map_hide", "manual map hide", Family::Delivery, "T1",
           "Red multi-step lab path for manual_map_hide",
           "Blue multi-reason lab path for manual_map_hide"},
          run};
}
}  // namespace strategies
