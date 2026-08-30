#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::secure_kernel_view {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:secure_kernel_view] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:secure_kernel_view] signal 2: evaluate strategy-specific posture\n");
  if (w.trust.secure_kernel_view_dirty) {
    sim::strategy_example::add_signal(outcome, "secure-kernel view reports tampering", 0.45);
  }
  std::printf("[blue:secure_kernel_view] signal 3: corroborate independent residual\n");
  if (w.trust.guest_ac_view_clean) {
    sim::strategy_example::add_signal(outcome, "guest self-check disagrees with secure view", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("secure_kernel_view", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "secure_kernel_view blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

BlueResult run_blue(sim::World& w, sim::Narrator& n) {
  n.counter(sim::Side::Blue, "secure_kernel_view", "Correlating independent lab signals.");
  auto result = detect(w);
  n.say(sim::Side::Blue, result.detail);
  return result;
}

}  // namespace examples::secure_kernel_view
