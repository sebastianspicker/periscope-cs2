// BLUE: on-target fires with mean latency below human reaction floor.

#include "blue_example.hpp"

#include <sstream>

namespace examples::triggerbot_timing {

BlueResult detect(sim::World& w) {
  BlueResult r;
  int fires = 0;
  float sum = 0.f;
  for (const auto& s : w.aim_samples) {
    if (s.crosshair_on_enemy && s.fired) {
      ++fires;
      sum += s.fire_latency_ms;
    }
  }
  r.on_target_fires = fires;
  r.mean_latency_ms = fires ? sum / static_cast<float>(fires) : 999.f;
  // Human reaction floor lab model: < 50ms mean on pure on-target fires is bot-like.
  const bool superhuman = fires >= 4 && r.mean_latency_ms < 50.f;
  r.detected = w.triggerbot_active && superhuman;
  r.mitigated = r.detected;
  if (r.mitigated) {
    w.triggerbot_active = false;
    w.ranked_access_denied = true;
    w.lab_confidence += 2.0;
  }
  std::ostringstream oss;
  oss << "triggerbot_timing blue fires=" << r.on_target_fires
      << " mean_lat_ms=" << r.mean_latency_ms
      << " superhuman=" << (superhuman ? 1 : 0);
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::triggerbot_timing
