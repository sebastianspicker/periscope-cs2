#include "blue_example.hpp"
#include <cstdio>

namespace examples::forensic_cleanup_exit {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.forensic_cleanup_active) {
    result.reasons.emplace_back("forensic cleanup active");
    result.cleanup_detected = true;
  }
  if (w.forensic_cleanup_steps_completed > 0) result.reasons.emplace_back("cleanup steps completed=" + std::to_string(w.forensic_cleanup_steps_completed));
  if (w.forensic_prefetch_cleared) result.reasons.emplace_back("prefetch artifacts cleared");
  if (w.forensic_recent_cleared) result.reasons.emplace_back("recent documents cleared");

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.cleanup_detected;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T0 forensic_cleanup_exit] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::forensic_cleanup_exit
