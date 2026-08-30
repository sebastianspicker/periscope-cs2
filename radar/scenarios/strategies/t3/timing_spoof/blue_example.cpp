#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::timing_spoof {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:timing_spoof] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:timing_spoof] signal 2: evaluate strategy-specific posture\n");
  if (w.trust.timing_spoofed) {
    sim::strategy_example::add_signal(outcome, "timing source is marked spoofed", 0.45);
  }
  std::printf("[blue:timing_spoof] signal 3: corroborate independent residual\n");
  if (w.trust.baseline_latency_ns > w.trust.cpuid_latency_ns * 2.0) {
    sim::strategy_example::add_signal(outcome, "cross-source timing measurements disagree", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("timing_spoof", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "timing_spoof blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

BlueResult run_blue(sim::World& w, sim::Narrator& n) {
  n.counter(sim::Side::Blue, "timing_spoof", "Correlating independent lab signals.");
  auto result = detect(w);
  n.say(sim::Side::Blue, result.detail);
  return result;
}

}  // namespace examples::timing_spoof
