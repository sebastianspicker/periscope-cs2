#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `capture_cv_hid`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::capture_cv_hid::apply(w);
  const auto blue = examples::capture_cv_hid::detect(w);

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

StrategyEntry entry_39_capture_cv_hid() {
  return {{ "39_capture_cv_hid", "capture cv hid", Family::Delivery, "T4",
           "Red multi-step lab path for capture_cv_hid",
           "Blue multi-reason lab path for capture_cv_hid"},
          run};
}
}  // namespace strategies
