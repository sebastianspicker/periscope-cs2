#include "blue_example.hpp"
#include <cstdio>

namespace examples::decoy_render_ml_evasion {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.decoy_render_active) {
    result.reasons.emplace_back("decoy render active");
    result.ml_confusion_detected = true;
  }
  if (w.ml_confusion_active) result.reasons.emplace_back("ML confusion engine active");
  if (w.decoy_frame_count > 0) result.reasons.emplace_back("decoy frames rendered=" + std::to_string(w.decoy_frame_count));
  if (w.ml_decoy_patterns_generated > 0) result.reasons.emplace_back("ML decoy patterns=" + std::to_string(w.ml_decoy_patterns_generated));

  for (const auto& [pid, proc] : w.processes) {
    if (proc.timing_jittered) result.reasons.emplace_back("timing jitter in pid=" + std::to_string(pid));
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.ml_confusion_detected;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T1 decoy_render_ml_evasion] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::decoy_render_ml_evasion
