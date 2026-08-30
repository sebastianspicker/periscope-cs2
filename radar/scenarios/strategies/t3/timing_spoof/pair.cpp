#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `timing_spoof`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::timing_spoof::run_red(w, n);
  const auto blue = examples::timing_spoof::run_blue(w, n);

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

StrategyEntry entry_38_timing_spoof() {
  return {{ "38_timing_spoof", "timing spoof", Family::Delivery, "T3",
           "Red multi-step lab path for timing_spoof",
           "Blue multi-reason lab path for timing_spoof"},
          run};
}
}  // namespace strategies
