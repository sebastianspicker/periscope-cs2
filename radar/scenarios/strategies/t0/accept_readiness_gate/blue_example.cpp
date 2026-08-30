#include "blue_example.hpp"
#include <cstdio>

namespace examples::accept_readiness_gate {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.accept_gate_open) {
    result.reasons.emplace_back("accept gate is open");
    result.readiness_bypassed = true;
  }
  if (w.accept_session_ready) result.reasons.emplace_back("session marked ready before expected");
  if (w.accept_conditions_met == w.accept_conditions_total) result.reasons.emplace_back("all " + std::to_string(w.accept_conditions_met) + "/" + std::to_string(w.accept_conditions_total) + " conditions met");

  result.signals = static_cast<int>(result.reasons.size());
  // Multi-reason: readiness bypass plus at least one corroborating condition.
  result.detected = result.signals >= 2 && result.readiness_bypassed;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T0 accept_readiness_gate] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::accept_readiness_gate
