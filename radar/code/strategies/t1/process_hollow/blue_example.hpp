#pragma once

// Lab BLUE example for strategy `process_hollow` on sim::World only.
// Multi-reason educational detect/mitigate on World scars.

#include "sim/world.hpp"

#include <string>
#include <vector>

namespace examples::process_hollow {

// Lab result/type `BlueResult` used by this educational unit.
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};

/// Multi-reason blue entry for `process_hollow`.
BlueResult detect(sim::World& w);

}  // namespace examples::process_hollow
