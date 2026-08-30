#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `fallback_chain`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::fallback_chain::apply(w);
  const auto blue = examples::fallback_chain::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_20_fallback_chain() {
  return {{ "20_fallback_chain", "fallback chain", Family::Structural, "all",
           "Red multi-step lab path for fallback_chain",
           "Blue multi-reason lab path for fallback_chain"},
          run};
}
}  // namespace strategies
