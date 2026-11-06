#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `etw_ti_blind`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::etw_ti_blind::run_red(w, n);
  const auto blue = examples::etw_ti_blind::run_blue(w, n);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_93_etw_ti_blind() {
  return {{ "93_etw_ti_blind", "etw ti blind", Family::Delivery, "T2",
           "Red multi-step lab path for etw_ti_blind",
           "Blue multi-reason lab path for etw_ti_blind"},
          run};
}
}  // namespace strategies
