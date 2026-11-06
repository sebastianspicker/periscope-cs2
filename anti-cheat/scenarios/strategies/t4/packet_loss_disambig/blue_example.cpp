#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::packet_loss_disambig {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:packet_loss_disambig] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:packet_loss_disambig] signal 2: evaluate strategy-specific posture\n");
  if (w.packet_loss_faked) {
    sim::strategy_example::add_signal(outcome, "steady synthetic packet loss is active", 0.45);
  }
  std::printf("[blue:packet_loss_disambig] signal 3: corroborate independent residual\n");
  if (!w.lag_switch_active) {
    sim::strategy_example::add_signal(outcome, "loss pattern lacks lag-switch bursts", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("packet_loss_disambig", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "packet_loss_disambig blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

}  // namespace examples::packet_loss_disambig
