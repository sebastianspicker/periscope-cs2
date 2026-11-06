#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::aim_challenge {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:aim_challenge] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:aim_challenge] signal 2: evaluate strategy-specific posture\n");
  if (w.silent_aim_active) {
    sim::strategy_example::add_signal(outcome, "silent aim residual is active", 0.45);
  }
  std::printf("[blue:aim_challenge] signal 3: corroborate independent residual\n");
  if (!w.aim_samples.empty() && !w.aim_samples.back().challenge_passed) {
    sim::strategy_example::add_signal(outcome, "server aim challenge failed", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("aim_challenge", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "aim_challenge blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

}  // namespace examples::aim_challenge
