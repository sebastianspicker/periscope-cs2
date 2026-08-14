#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::hvci_race {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:hvci_race] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:hvci_race] signal 2: evaluate strategy-specific posture\n");
  if (!w.trust.hvci) {
    sim::strategy_example::add_signal(outcome, "HVCI policy is disabled", 0.45);
  }
  std::printf("[blue:hvci_race] signal 3: corroborate independent residual\n");
  if (!w.trust.hvci_enabled) {
    sim::strategy_example::add_signal(outcome, "runtime HVCI state is disabled", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("hvci_race", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "hvci_race blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

BlueResult run_blue(sim::World& w, sim::Narrator& n) {
  n.counter(sim::Side::Blue, "hvci_race", "Correlating independent lab signals.");
  auto result = detect(w);
  n.say(sim::Side::Blue, result.detail);
  return result;
}

}  // namespace examples::hvci_race
