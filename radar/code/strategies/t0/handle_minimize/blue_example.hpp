#pragma once

// Blue simulation for handle_minimize. It checks independent handle/read telemetry and the
// strategy-specific scar (brief VM_READ handle) before reporting a risk-scored detection.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::handle_minimize {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::handle_minimize
