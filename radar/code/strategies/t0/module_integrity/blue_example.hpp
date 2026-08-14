#pragma once

// Blue simulation for module_integrity. It checks independent handle/read telemetry and the
// strategy-specific scar (patched executable module) before reporting a risk-scored detection.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::module_integrity {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::module_integrity
