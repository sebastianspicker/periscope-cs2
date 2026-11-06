#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::feature_control_msr {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:feature_control_msr] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:feature_control_msr] signal 2: evaluate strategy-specific posture\n");
  if (w.feature_control_spoofed) {
    sim::strategy_example::add_signal(outcome, "feature-control state is inconsistent", 0.45);
  }
  std::printf("[blue:feature_control_msr] signal 3: corroborate independent residual\n");
  if (w.trust.personal_hv_active) {
    sim::strategy_example::add_signal(outcome, "unexpected hypervisor is active", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("feature_control_msr", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "feature_control_msr blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

BlueResult run_blue(sim::World& w, sim::Narrator& n) {
  n.counter(sim::Side::Blue, "feature_control_msr", "Correlating independent lab signals.");
  auto result = detect(w);
  n.say(sim::Side::Blue, result.detail);
  return result;
}

}  // namespace examples::feature_control_msr
