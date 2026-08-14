#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  auto red = examples::ept_violation_evade::apply(w);
  auto blue = examples::ept_violation_evade::detect(w);
  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.80;
  r.summary = red.detail + " | blue=" + std::to_string(blue.signals) +
              "sig risk=" + std::to_string(blue.risk);
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_70_ept_violation_evade() {
  return {{"70_ept_violation_evade", "EPT violation side-channel evade", Family::Evasion, "T3",
           "Red uses EPT violation timing to detect and evade AC memory scans",
           "Blue detects via INVEPT residual + HV probe (near impossible)"}, run};
}

}  // namespace strategies
