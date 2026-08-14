#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `inmatch_only`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::inmatch_only::apply(w);
  const auto blue = examples::inmatch_only::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.72;
  r.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(r.blue_detected, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_32_inmatch_only() {
  return {{ "32_inmatch_only", "inmatch only", Family::Delivery, "T1",
           "Red multi-step lab path for inmatch_only",
           "Blue multi-reason lab path for inmatch_only"},
          run};
}
}  // namespace strategies
