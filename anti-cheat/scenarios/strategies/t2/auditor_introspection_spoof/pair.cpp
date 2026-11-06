#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T2 evasion pair. Red rewrites the CRC the in-game auditor's module scan
// reports, nulls diagnostic dumps overlapping its image, and feeds original
// prologue bytes back through the string-copy path; blue answers with
// CRC-of-CRC verification, second-path prologue reads, and dump cross-checks.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  // Lab precondition: the auditor (CS2 diagnostic module scan) is active.
  w.diagnostic_system_active = true;
  w.diagnostic_state.is_active = true;
  w.diagnostic_state.has_focus = true;
  const auto red = examples::auditor_introspection_spoof::apply(w);
  const auto blue = examples::auditor_introspection_spoof::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals) +
                   " risk=" + std::to_string(blue.risk);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_157_auditor_introspection_spoof() {
  return {{"157_auditor_introspection_spoof", "auditor introspection spoof", Family::Evasion,
           "T1-T2",
           "Red spoofs the auditor's module-scan CRC, dumps, and prologue reads",
           "Blue verifies CRC-of-CRC, second-path prologue reads, and dump cross-checks"},
          run};
}
}  // namespace strategies
