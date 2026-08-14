#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::clipcursor {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:clipcursor] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:clipcursor] signal 2: evaluate strategy-specific posture\n");
  if (w.clipcursor_confined) {
    sim::strategy_example::add_signal(outcome, "cursor confinement is active", 0.45);
  }
  std::printf("[blue:clipcursor] signal 3: corroborate independent residual\n");
  if (!w.inputs.empty() && w.inputs.back().source == "injected") {
    sim::strategy_example::add_signal(outcome, "injected pointer input is observed", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("clipcursor", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "clipcursor blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

}  // namespace examples::clipcursor
