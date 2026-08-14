#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `raw_vs_sendinput`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::raw_vs_sendinput::apply(w);
  const auto blue = examples::raw_vs_sendinput::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.72;
  r.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(r.blue_detected, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_91_raw_vs_sendinput() {
  return {{ "91_raw_vs_sendinput", "raw vs sendinput", Family::Delivery, "T1",
           "Red multi-step lab path for raw_vs_sendinput",
           "Blue multi-reason lab path for raw_vs_sendinput"},
          run};
}
}  // namespace strategies
