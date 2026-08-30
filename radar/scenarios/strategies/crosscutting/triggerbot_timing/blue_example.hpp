#pragma once
// Lab BLUE: superhuman on-target fire latency residual.
#include "sim/world.hpp"
#include <string>
namespace examples::triggerbot_timing {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int on_target_fires = 0;
  float mean_latency_ms = 0.f;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::triggerbot_timing
