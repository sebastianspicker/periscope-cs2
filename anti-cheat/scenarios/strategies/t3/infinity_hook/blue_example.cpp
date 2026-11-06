#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::infinity_hook {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:infinity_hook] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:infinity_hook] signal 2: evaluate strategy-specific posture\n");
  if (w.infinity_hook_residual) {
    sim::strategy_example::add_signal(outcome, "kernel hook residual is present", 0.45);
  }
  std::printf("[blue:infinity_hook] signal 3: corroborate independent residual\n");
  if (w.etw_ti_blind) {
    sim::strategy_example::add_signal(outcome, "ETW threat-intelligence telemetry is blind", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("infinity_hook", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "infinity_hook blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

BlueResult run_blue(sim::World& w, sim::Narrator& n) {
  n.counter(sim::Side::Blue, "infinity_hook", "Correlating independent lab signals.");
  auto result = detect(w);
  n.say(sim::Side::Blue, result.detail);
  return result;
}

}  // namespace examples::infinity_hook
