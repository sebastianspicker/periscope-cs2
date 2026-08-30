#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::desktop_duplication {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:desktop_duplication] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:desktop_duplication] signal 2: evaluate strategy-specific posture\n");
  if (w.desktop_duplication) {
    sim::strategy_example::add_signal(outcome, "desktop-duplication capture is active", 0.45);
  }
  std::printf("[blue:desktop_duplication] signal 3: corroborate independent residual\n");
  if (w.capture_sensor_active) {
    sim::strategy_example::add_signal(outcome, "capture sensor reports a duplication path", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("desktop_duplication", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "desktop_duplication blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

}  // namespace examples::desktop_duplication
