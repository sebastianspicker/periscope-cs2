#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `feature_control_msr`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::feature_control_msr::run_red(w, n);
  const auto blue = examples::feature_control_msr::run_blue(w, n);

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

StrategyEntry entry_83_feature_control_msr() {
  return {{ "83_feature_control_msr", "feature control msr", Family::Delivery, "T3",
           "Red multi-step lab path for feature_control_msr",
           "Blue multi-reason lab path for feature_control_msr"},
          run};
}
}  // namespace strategies
