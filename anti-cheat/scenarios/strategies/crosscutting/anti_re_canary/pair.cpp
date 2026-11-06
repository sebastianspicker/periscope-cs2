#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::anti_re_canary::apply(w);
  const auto blue = examples::anti_re_canary::detect(w);
  StrategyResult r{red.achieved, blue.detected, blue.mitigated, red.detail + " | " + blue.detail};
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}
}  // namespace
StrategyEntry entry_108_anti_re_canary() {
  return {{"108_anti_re_canary", "anti-RE canary", Family::Evasion, "all",
           "Model canary and debugger-state checks in the simulator",
           "Detect contradictory anti-analysis state"}, run};
}
}  // namespace strategies
