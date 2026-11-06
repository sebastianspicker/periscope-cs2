#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `etw_blind`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::etw_blind::apply(w);
  const auto blue = examples::etw_blind::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.72;
  r.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(r.blue_detected, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_45_etw_blind() {
  return {{ "45_etw_blind", "etw blind", Family::Delivery, "T1",
           "Red multi-step lab path for etw_blind",
           "Blue multi-reason lab path for etw_blind"},
          run};
}
}  // namespace strategies
