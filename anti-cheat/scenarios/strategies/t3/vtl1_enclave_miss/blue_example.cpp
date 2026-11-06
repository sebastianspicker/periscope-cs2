#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::vtl1_enclave_miss {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:vtl1_enclave_miss] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:vtl1_enclave_miss] signal 2: evaluate strategy-specific posture\n");
  if (!w.vtl1_enclave_present) {
    sim::strategy_example::add_signal(outcome, "expected VTL1 enclave is absent", 0.45);
  }
  std::printf("[blue:vtl1_enclave_miss] signal 3: corroborate independent residual\n");
  if (!w.trust.vbs) {
    sim::strategy_example::add_signal(outcome, "VBS is disabled", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("vtl1_enclave_miss", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "vtl1_enclave_miss blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

BlueResult run_blue(sim::World& w, sim::Narrator& n) {
  n.counter(sim::Side::Blue, "vtl1_enclave_miss", "Correlating independent lab signals.");
  auto result = detect(w);
  n.say(sim::Side::Blue, result.detail);
  return result;
}

}  // namespace examples::vtl1_enclave_miss
