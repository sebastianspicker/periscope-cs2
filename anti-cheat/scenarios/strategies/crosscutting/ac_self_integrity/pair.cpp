#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `ac_self_integrity`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::ac_self_integrity::apply(w);
  const auto blue = examples::ac_self_integrity::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_41_ac_self_integrity() {
  return {{ "41_ac_self_integrity", "ac self integrity", Family::Detection, "all",
           "Red multi-step lab path for ac_self_integrity",
           "Blue multi-reason lab path for ac_self_integrity"},
          run};
}
}  // namespace strategies
