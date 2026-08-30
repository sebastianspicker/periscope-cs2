#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `desktop_duplication`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::desktop_duplication::apply(w);
  const auto blue = examples::desktop_duplication::detect(w);

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

StrategyEntry entry_72_desktop_duplication() {
  return {{ "72_desktop_duplication", "desktop duplication", Family::Delivery, "T4",
           "Red multi-step lab path for desktop_duplication",
           "Blue multi-reason lab path for desktop_duplication"},
          run};
}
}  // namespace strategies
