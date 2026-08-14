#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::lag_switch {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:lag_switch] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:lag_switch] signal 2: evaluate strategy-specific posture\n");
  if (w.lag_switch_active) {
    sim::strategy_example::add_signal(outcome, "intentional drop-burst pattern is active", 0.45);
  }
  std::printf("[blue:lag_switch] signal 3: corroborate independent residual\n");
  if (w.lag_switch_drop_bursts >= 2) {
    sim::strategy_example::add_signal(outcome, "multiple synchronized drop bursts were recorded", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("lag_switch", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "lag_switch blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

}  // namespace examples::lag_switch
