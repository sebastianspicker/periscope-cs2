#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `external_clone_display`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::external_clone_display::apply(w);
  const auto blue = examples::external_clone_display::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.65;
  r.summary = red.detail + " | " + blue.detail + " | signals=" +
              std::to_string(blue.signals) + " risk=" + std::to_string(blue.risk);
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_85_external_clone_display() {
  return {{ "85_external_clone_display", "external clone display", Family::Delivery, "T4",
           "Red multi-step lab path for external_clone_display",
           "Blue multi-reason lab path for external_clone_display"},
          run};
}
}  // namespace strategies
