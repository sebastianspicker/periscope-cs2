#pragma once

// Blue simulation for printwindow. It checks independent handle/read telemetry and the
// strategy-specific scar (PrintWindow capture) before reporting a risk-scored detection.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::printwindow {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::printwindow
