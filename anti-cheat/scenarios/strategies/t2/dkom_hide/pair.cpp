#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `dkom_hide`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::dkom_hide::run_red(w, n);
  const auto blue = examples::dkom_hide::run_blue(w, n);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_68_dkom_hide() {
  return {{ "68_dkom_hide", "dkom hide", Family::Delivery, "T2",
           "Red multi-step lab path for dkom_hide",
           "Blue multi-reason lab path for dkom_hide"},
          run};
}
}  // namespace strategies
