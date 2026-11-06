#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::dual_boot_posture {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:dual_boot_posture] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:dual_boot_posture] signal 2: evaluate strategy-specific posture\n");
  if (w.trust.dual_boot_profile) {
    sim::strategy_example::add_signal(outcome, "alternate boot profile is present", 0.45);
  }
  std::printf("[blue:dual_boot_posture] signal 3: corroborate independent residual\n");
  if (w.trust.boot_pcr_profile != "known_good") {
    sim::strategy_example::add_signal(outcome, "PCR profile differs from baseline", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("dual_boot_posture", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "dual_boot_posture blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

}  // namespace examples::dual_boot_posture
