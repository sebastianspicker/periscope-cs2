#include "blue_example.hpp"
#include <cstdio>

namespace examples::runtime_health_ladder {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.health_ladder_level > 0) {
    result.reasons.emplace_back("health ladder level=" + std::to_string(w.health_ladder_level));
    result.health_ladder_detected = true;
  }
  if (w.health_ladder_transition_count > 0) result.reasons.emplace_back("health ladder transitions=" + std::to_string(w.health_ladder_transition_count));
  if (w.health_ladder_self_heal_armed) result.reasons.emplace_back("health ladder self-heal armed");
  if (w.health_ladder_ticks_at_level > 0) result.reasons.emplace_back("health ladder ticks at level=" + std::to_string(w.health_ladder_ticks_at_level));

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.health_ladder_detected;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T0 runtime_health_ladder] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::runtime_health_ladder
