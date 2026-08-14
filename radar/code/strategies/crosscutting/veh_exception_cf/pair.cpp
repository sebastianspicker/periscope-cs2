#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `veh_exception_cf`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::veh_exception_cf::apply(w);
  const auto blue = examples::veh_exception_cf::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_63_veh_exception_cf() {
  return {{ "63_veh_exception_cf", "veh exception cf", Family::Evasion, "all",
           "Red multi-step lab path for veh_exception_cf",
           "Blue multi-reason lab path for veh_exception_cf"},
          run};
}
}  // namespace strategies
