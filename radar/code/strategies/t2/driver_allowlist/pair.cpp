#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `driver_allowlist`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::driver_allowlist::run_red(w, n);
  const auto blue = examples::driver_allowlist::run_blue(w, n);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_33_driver_allowlist() {
  return {{ "33_driver_allowlist", "driver allowlist", Family::Delivery, "T2",
           "Red multi-step lab path for driver_allowlist",
           "Blue multi-reason lab path for driver_allowlist"},
          run};
}
}  // namespace strategies
