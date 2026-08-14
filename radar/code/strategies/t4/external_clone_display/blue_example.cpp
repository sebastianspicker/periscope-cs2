#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::external_clone_display {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:external_clone_display] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:external_clone_display] signal 2: evaluate strategy-specific posture\n");
  if (w.external_display_clone) {
    sim::strategy_example::add_signal(outcome, "external display cloning is active", 0.45);
  }
  std::printf("[blue:external_clone_display] signal 3: corroborate independent residual\n");
  if (w.capture_vs_present_mismatch) {
    sim::strategy_example::add_signal(outcome, "capture and present paths disagree", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("external_clone_display", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "external_clone_display blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

}  // namespace examples::external_clone_display
