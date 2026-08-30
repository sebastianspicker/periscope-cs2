#pragma once

// Blue simulation for read_throttle. It checks independent handle/read telemetry and the
// strategy-specific scar (brief, throttled remote read) before reporting a risk-scored detection.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::read_throttle {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::read_throttle
