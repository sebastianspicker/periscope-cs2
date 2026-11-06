#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::nested_hv {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:nested_hv] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:nested_hv] signal 2: evaluate strategy-specific posture\n");
  if (w.trust.personal_hv_active && w.trust.platform_hv_active) {
    sim::strategy_example::add_signal(outcome, "nested hypervisor posture is active", 0.45);
  }
  std::printf("[blue:nested_hv] signal 3: corroborate independent residual\n");
  if (w.trust.hv_vendor == "NestedLabHV") {
    sim::strategy_example::add_signal(outcome, "nested hypervisor vendor is exposed", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("nested_hv", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "nested_hv blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

BlueResult run_blue(sim::World& w, sim::Narrator& n) {
  n.counter(sim::Side::Blue, "nested_hv", "Correlating independent lab signals.");
  auto result = detect(w);
  n.say(sim::Side::Blue, result.detail);
  return result;
}

}  // namespace examples::nested_hv
