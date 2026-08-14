#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::network_multibox_aim {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:network_multibox_aim] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:network_multibox_aim] signal 2: evaluate strategy-specific posture\n");
  if (w.multibox_net_aim) {
    sim::strategy_example::add_signal(outcome, "remote multibox aim stream is active", 0.45);
  }
  std::printf("[blue:network_multibox_aim] signal 3: corroborate independent residual\n");
  if (w.multibox_input_desync) {
    sim::strategy_example::add_signal(outcome, "input timing diverges from local aim stream", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("network_multibox_aim", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "network_multibox_aim blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

}  // namespace examples::network_multibox_aim
