#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `dual_boot_posture`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::dual_boot_posture::apply(w);
  const auto blue = examples::dual_boot_posture::detect(w);

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

StrategyEntry entry_50_dual_boot_posture() {
  return {{ "50_dual_boot_posture", "dual boot posture", Family::Delivery, "T4",
           "Red multi-step lab path for dual_boot_posture",
           "Blue multi-reason lab path for dual_boot_posture"},
          run};
}
}  // namespace strategies
