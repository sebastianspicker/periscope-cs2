#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::cr3_stealth_target {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:cr3_stealth_target] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:cr3_stealth_target] signal 2: evaluate strategy-specific posture\n");
  if (w.trust.personal_hv_active) {
    sim::strategy_example::add_signal(outcome, "unexpected hypervisor is active", 0.45);
  }
  std::printf("[blue:cr3_stealth_target] signal 3: corroborate independent residual\n");
  if (w.proc(w.game_pid()) != nullptr && w.proc(w.game_pid())->cr3 != 0) {
    sim::strategy_example::add_signal(outcome, "game has an abnormal CR3 token", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("cr3_stealth_target", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "cr3_stealth_target blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

BlueResult run_blue(sim::World& w, sim::Narrator& n) {
  n.counter(sim::Side::Blue, "cr3_stealth_target", "Correlating independent lab signals.");
  auto result = detect(w);
  n.say(sim::Side::Blue, result.detail);
  return result;
}

}  // namespace examples::cr3_stealth_target
