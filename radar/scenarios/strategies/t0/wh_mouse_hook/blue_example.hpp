#pragma once

// Blue simulation for wh_mouse_hook. It checks independent handle/read telemetry and the
// strategy-specific scar (global mouse hook) before reporting a risk-scored detection.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::wh_mouse_hook {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::wh_mouse_hook
