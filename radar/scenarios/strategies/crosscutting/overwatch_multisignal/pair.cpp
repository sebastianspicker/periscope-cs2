#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `overwatch_multisignal`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::overwatch_multisignal::apply(w);
  const auto blue = examples::overwatch_multisignal::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_98_overwatch_multisignal() {
  return {{ "98_overwatch_multisignal", "overwatch multisignal", Family::Detection, "all",
           "Red multi-step lab path for overwatch_multisignal",
           "Blue multi-reason lab path for overwatch_multisignal"},
          run};
}
}  // namespace strategies
