#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `process_hollow`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::process_hollow::apply(w);
  const auto blue = examples::process_hollow::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.72;
  r.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(r.blue_detected, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_55_process_hollow() {
  return {{ "55_process_hollow", "process hollow", Family::Delivery, "T1",
           "Red multi-step lab path for process_hollow",
           "Blue multi-reason lab path for process_hollow"},
          run};
}
}  // namespace strategies
