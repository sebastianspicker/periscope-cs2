#pragma once

// Lab BLUE example for strategy `manual_map_hide` on sim::World only.
// Multi-reason educational detect/mitigate on World scars.

#include "sim/world.hpp"

#include <string>
#include <vector>

namespace examples::manual_map_hide {

// Lab result/type `BlueResult` used by this educational unit.
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};

/// Multi-reason blue entry for `manual_map_hide`.
BlueResult detect(sim::World& w);

}  // namespace examples::manual_map_hide
