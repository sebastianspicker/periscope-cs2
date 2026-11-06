#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `overwatch_queue`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::overwatch_queue::apply(w);
  const auto blue = examples::overwatch_queue::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_74_overwatch_queue() {
  return {{ "74_overwatch_queue", "overwatch queue", Family::Detection, "all",
           "Red multi-step lab path for overwatch_queue",
           "Blue multi-reason lab path for overwatch_queue"},
          run};
}
}  // namespace strategies
