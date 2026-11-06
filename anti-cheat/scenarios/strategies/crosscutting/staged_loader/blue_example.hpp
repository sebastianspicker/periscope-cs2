#pragma once

// Lab BLUE example for strategy `staged_loader` on sim::World only.
// Multi-reason educational detect/mitigate on World scars.

#include "sim/world.hpp"

#include <string>
#include <vector>

namespace examples::staged_loader {

struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  std::vector<std::string> reasons;
  std::string detail;
};

/// Multi-reason blue entry for `staged_loader`.
BlueResult detect(sim::World& w);

}  // namespace examples::staged_loader
