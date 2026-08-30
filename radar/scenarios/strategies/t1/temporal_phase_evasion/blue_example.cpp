#include "blue_example.hpp"
#include <cstdio>

namespace examples::temporal_phase_evasion {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.temporal_phase_active) {
    result.reasons.emplace_back("temporal jitter engine active");
    result.temporal_detected = true;
  }
  if (w.temporal_phase_transitions > 0) result.reasons.emplace_back("phase transitions=" + std::to_string(w.temporal_phase_transitions));
  if (w.temporal_phase_ticks_in_phase > 0) result.reasons.emplace_back("ticks in phase=" + std::to_string(w.temporal_phase_ticks_in_phase));

  for (const auto& [pid, proc] : w.processes) {
    if (proc.timing_jittered) result.reasons.emplace_back("timing jitter detected in pid=" + std::to_string(pid));
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.temporal_detected;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T1 temporal_phase_evasion] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::temporal_phase_evasion
