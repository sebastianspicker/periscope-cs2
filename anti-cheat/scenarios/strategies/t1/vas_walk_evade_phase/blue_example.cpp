#include "blue_example.hpp"
#include <cstdio>

namespace examples::vas_walk_evade_phase {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.vas_walk_evade_phase_active) {
    result.reasons.emplace_back("VAS walk evade phase active");
    result.vas_evasion_detected = true;
  }
  if (w.vas_walk_evasion_count > 0) result.reasons.emplace_back("VAS walk evasions=" + std::to_string(w.vas_walk_evasion_count));
  if (w.vas_page_hidden) result.reasons.emplace_back("VAS page hidden from enumeration");
  if (w.vas_region_reshuffled) result.reasons.emplace_back("VAS region reshuffled");

  for (const auto& [pid, proc] : w.processes) {
    if (proc.hidden_from_weak_enum) result.reasons.emplace_back("process hidden from enum pid=" + std::to_string(pid));
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.vas_evasion_detected;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T1 vas_walk_evade_phase] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::vas_walk_evade_phase
