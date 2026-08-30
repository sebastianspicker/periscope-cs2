#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `report_velocity`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::report_velocity::apply(w);
  const auto blue = examples::report_velocity::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_51_report_velocity() {
  return {{ "51_report_velocity", "report velocity", Family::Detection, "all",
           "Red multi-step lab path for report_velocity",
           "Blue multi-reason lab path for report_velocity"},
          run};
}
}  // namespace strategies
