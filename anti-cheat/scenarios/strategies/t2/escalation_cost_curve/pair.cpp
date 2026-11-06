#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T2 detection pair. Red walks the access rungs (RPM -> syscall -> kernel
// driver); each rung leaves distinct persistent system mutations (testsigning
// flag, HVCI/blocklist registry toggles, pending reboot). Blue observes the
// mutation footprints and prices the escalation into a ranked deny.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::escalation_cost_curve::apply(w);
  const auto blue = examples::escalation_cost_curve::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals) +
                   " risk=" + std::to_string(blue.risk);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_164_escalation_cost_curve() {
  return {{"164_escalation_cost_curve", "escalation cost curve", Family::Detection,
           "T1-T2",
           "Red climbs RPM -> syscall -> kernel rungs, each leaving persistent mutations",
           "Blue observes mutation footprints and prices the escalation"},
          run};
}
}  // namespace strategies
