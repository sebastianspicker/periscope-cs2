#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `aim_humanization`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::aim_humanization::apply(w);
  const auto blue = examples::aim_humanization::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_11_aim_humanization() {
  return {{ "11_aim_humanization", "aim humanization", Family::Feature, "all",
           "Red multi-step lab path for aim_humanization",
           "Blue multi-reason lab path for aim_humanization"},
          run};
}
}  // namespace strategies
