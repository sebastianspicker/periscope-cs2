// RED: fire only when crosshair on enemy with sub-human reaction latency.

#include "red_example.hpp"

#include <sstream>

namespace examples::triggerbot_timing {

RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("triggerbot.exe");
  // No soft aim wander — pure trigger residual.
  float sum = 0.f;
  int n = 0;
  for (int i = 0; i < 8; ++i) {
    sim::AimSample s;
    s.crosshair_on_enemy = true;
    s.fired = true;
    s.fire_latency_ms = 8.f + static_cast<float>(i % 3);  // ~8–10ms
    s.has_vision_on_target = true;
    w.aim_samples.push_back(s);
    sum += s.fire_latency_ms;
    ++n;
  }
  r.fires = n;
  r.mean_latency_ms = n ? sum / static_cast<float>(n) : 0.f;
  w.triggerbot_active = true;
  w.trigger_on_target_fires = n;
  w.trigger_mean_latency_ms = r.mean_latency_ms;

  r.achieved = w.triggerbot_active && w.trigger_on_target_fires >= 6 &&
               w.trigger_mean_latency_ms < 20.f;
  std::ostringstream oss;
  oss << "triggerbot_timing red fires=" << r.fires
      << " mean_lat_ms=" << r.mean_latency_ms;
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::triggerbot_timing
