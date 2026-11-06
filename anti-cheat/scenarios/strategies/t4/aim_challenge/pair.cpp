#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `aim_challenge`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::aim_challenge::apply(w);
  const auto blue = examples::aim_challenge::detect(w);

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

StrategyEntry entry_60_aim_challenge() {
  return {{ "60_aim_challenge", "aim challenge", Family::Delivery, "T4",
           "Red multi-step lab path for aim_challenge",
           "Blue multi-reason lab path for aim_challenge"},
          run};
}
}  // namespace strategies
