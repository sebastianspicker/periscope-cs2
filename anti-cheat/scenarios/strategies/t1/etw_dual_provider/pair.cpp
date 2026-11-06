// etw_dual_provider — multi-step red blinds TI; multi-reason blue keeps secondary.

#include "strategies/framework.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Red, "Multi-step: blind primary ETW TI then open/read game.");
  strategy::t1_etw_dual_provider::Red red;
  red.apply(w);

  n.say(sim::Side::Blue, "Multi-reason: TI health + secondary provider + handle graph.");
  strategy::t1_etw_dual_provider::Blue blue;
  auto det = blue.detect(w);
  auto mit = blue.mitigate(w);

  StrategyResult r;
  r.red_achieved = w.etw_ti_blind && !w.etw_enabled;
  r.blue_detected = det.detected || (det.signals >= 2 && det.ti_down);
  r.blue_mitigated = mit.mitigated || (mit.secondary_detected && w.etw_secondary_active);
  r.summary = det.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_116_etw_dual_provider() {
  return {{"116_etw_dual_provider", "ETW Dual Provider Counter-Blinding",
           Family::Evasion, "T1",
           "Multi-step blind ETW TI + open/read residual",
           "Multi-reason secondary provider + handle graph"},
          run};
}

}  // namespace strategies
